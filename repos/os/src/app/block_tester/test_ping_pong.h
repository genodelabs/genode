/*
 * \brief  Block session testing - ping pong test
 * \author Josef Soentgen
 * \date   2016-07-04
 */

/*
 * Copyright (C) 2016-2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _TEST_PING_PONG_H_
#define _TEST_PING_PONG_H_

namespace Test {
	struct Ping_pong;
}


/*
 * Ping_pong operation test
 *
 * This test reads or writes the given number of blocks from the
 * specified start block sequentially in sized requests.
 */
struct Test::Ping_pong : Test_base
{
	Genode::Env       &_env;
	Genode::Allocator &_alloc;

	Block::Packet_descriptor::Opcode _op {
		Block::Packet_descriptor::READ };

	/* block session infos */
	enum { TX_BUF_SIZE = 4 * 1024 * 1024, };
	Genode::Allocator_avl _block_alloc { &_alloc };
	Genode::Constructible<Block::Connection> _block { };

	Genode::Constructible<Timer::Connection> _timer { };

	/* test data */
	bool            _ping   { true };
	Block::sector_t _start  { 0 };
	Block::sector_t _end    { 0 };
	size_t          _length { 0 };
	size_t          _size   { 0 };

	size_t _blocks           { 0 };
	char   _scratch_buffer[1u<<20] { };

	Genode::Constructible<Timer::Periodic_timeout<Ping_pong>> _progress_timeout { };

	void _handle_progress_timeout(Genode::Duration)
	{
		Genode::log("progress: rx:", _rx, " tx:", _tx);
	}

	void _handle_submit()
	{
		try {
			while (_blocks < _length_in_blocks && _block->tx()->ready_to_submit()) {

				Block::Packet_descriptor tmp =
					_block->tx()->alloc_packet(_size_in_blocks * _block_size);

				Block::sector_t const lba = _ping ? _start + _blocks
				                                  : _end   - _blocks;
				_ping ^= true;

				Block::Packet_descriptor p(tmp,
					_op, lba, _size_in_blocks);

				/* simulate write */
				if (_op == Block::Packet_descriptor::WRITE) {
					char * const content = _block->tx()->packet_content(p);
					Genode::memcpy(content, _scratch_buffer, p.size());
				}

				_block->tx()->submit_packet(p);
				_blocks += _size_in_blocks;
			}
		} catch (...) { }
	}

	void _handle_ack()
	{
		if (_finished) { return; }

		while (_block->tx()->ack_avail()) {

			Block::Packet_descriptor p = _block->tx()->get_acked_packet();

			if (!p.succeeded()) {
				Genode::error("processing ", p.block_number(), " ",
				              p.block_count(), " failed");

				if (_stop_on_error) { throw Test_failed(); }
				else                { _finish(); }
			}

			size_t                           const psize = p.size();
			size_t                           const count = psize / _block_size;
			Block::Packet_descriptor::Opcode const op    = p.operation();

			/* simulate read */
			if (op == Block::Packet_descriptor::READ) {
				char * const content = _block->tx()->packet_content(p);
				Genode::memcpy(_scratch_buffer, content, p.size());
			}

			_rx += (op == Block::Packet_descriptor::READ)  * count;
			_tx += (op == Block::Packet_descriptor::WRITE) * count;

			_bytes += psize;
			_block->tx()->release_packet(p);
		}

		if (_bytes >= _length) {
			_success = true;
			_finish();
			return;
		}

		_handle_submit();
	}

	void _finish()
	{
		_end_time = _timer->elapsed_ms();

		Test_base::_finish();
	}

	Genode::Signal_handler<Ping_pong> _ack_sigh {
		_env.ep(), *this, &Ping_pong::_handle_ack };

	Genode::Signal_handler<Ping_pong> _submit_sigh {
		_env.ep(), *this, &Ping_pong::_handle_submit };

	Genode::Xml_node _node;

	/**
	 * Constructor
	 *
	 * \param block  Block session reference
	 * \param node   node containing the test configuration
	 */
	Ping_pong(Genode::Env &env, Genode::Allocator &alloc,
	           Genode::Xml_node node,
	           Genode::Signal_context_capability finished_sig)
	: Test_base(finished_sig), _env(env), _alloc(alloc), _node(node)
	{ }

	/********************
	 ** Test interface **
	 ********************/

	void start(bool stop_on_error) override
	{
		_stop_on_error = stop_on_error;

		_block.construct(_env, &_block_alloc, TX_BUF_SIZE);

		_block->tx_channel()->sigh_ack_avail(_ack_sigh);
		_block->tx_channel()->sigh_ready_to_submit(_submit_sigh);

		_block->info(&_block_count, &_block_size, &_block_ops);

		_start = _node.attribute_value("start", 0u);
		try {
			Genode::Number_of_bytes tmp;
			_node.attribute("size").value(&tmp);
			_size = tmp;

			_node.attribute("length").value(&tmp);
			_length = tmp;
		} catch (...) { }

		if (_size > sizeof(_scratch_buffer)) {
			Genode::error("request size exceeds scratch buffer size");
			throw Constructing_test_failed();
		}

		size_t const total_bytes = _block_count * _block_size;
		if (_length > total_bytes - (_start * _block_size)) {
			Genode::error("length too large invalid");
			throw Constructing_test_failed();
		}

		if (_block_size > _size || (_size % _block_size) != 0) {
			Genode::error("request size invalid");
			throw Constructing_test_failed();
		}

		if (_node.attribute_value("write", false)) {
			_op = Block::Packet_descriptor::WRITE;
		}

		_size_in_blocks   = _size   / _block_size;
		_length_in_blocks = _length / _block_size;
		_end              = _start  + _length_in_blocks;

		_timer.construct(_env);

		uint64_t const progress_interval = _node.attribute_value("progress", 0ul);
		if (progress_interval) {
			_progress_timeout.construct(*_timer, *this,
			                            &Ping_pong::_handle_progress_timeout,
			                            Genode::Microseconds(progress_interval*1000));
		}

		_start_time = _timer->elapsed_ms();
		_handle_submit();
	}

	Result finish() override
	{
		_timer.destruct();
		_block.destruct();

		return Result(_success, _end_time - _start_time,
		              _bytes, _rx, _tx, _size, _block_size);
	}

	char const *name() const { return "ping_pong"; }
};




#endif /* _TEST_PING_PONG_H_ */
