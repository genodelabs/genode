/*
 * \brief  Block session testing
 * \author Josef Soentgen
 * \date   2016-07-04
 */

/*
 * Copyright (C) 2016-2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _TEST_REPLAY_H_
#define _TEST_REPLAY_H_

namespace Test {
	struct Replay;
}

/*
 * Replay test
 *
 * This test replays a recorded sequence of Block session
 * requests.
 */
struct Test::Replay : Test_base
{
	Genode::Env       &env;
	Genode::Allocator &alloc;

	struct Request : Genode::Fifo<Request>::Element
	{
		Block::Packet_descriptor::Opcode op;
		Block::sector_t nr;
		Genode::size_t  count;

		Request(Block::Packet_descriptor::Opcode op,
		        Block::sector_t nr, Genode::size_t count)
		: op(op), nr(nr), count(count) { }
	};

	unsigned              request_num { 0 };
	Genode::Fifo<Request> requests { };

	char   _scratch_buffer[4u<<20] { };

	bool _bulk { false };

	Genode::Constructible<Timer::Connection> _timer { };

	enum { TX_BUF_SIZE = 4 * 1024 * 1024, };
	Genode::Allocator_avl _block_alloc { &alloc };
	Genode::Constructible<Block::Connection> _block { };

	Genode::Signal_handler<Replay> _ack_sigh {
		env.ep(), *this, &Replay::_handle_ack };

	Genode::Signal_handler<Replay> _submit_sigh {
		env.ep(), *this, &Replay::_handle_submit };

	void _handle_submit()
	{
		bool more = true;

		try {
			while (_block->tx()->ready_to_submit() && more) {
				/* peak at head ... */
				Request *req = requests.head();
				if (!req) { return; }

				Block::Packet_descriptor p(
					_block->tx()->alloc_packet(req->count * _block_size),
					req->op, req->nr, req->count);

				bool const write = req->op == Block::Packet_descriptor::WRITE;

				/* simulate write */
				if (write) {
					char * const content = _block->tx()->packet_content(p);
					Genode::memcpy(content, _scratch_buffer, p.size());
				}

				_block->tx()->submit_packet(p);

				/* ... and only remove it if we could submit it */
				req = requests.dequeue();
				Genode::destroy(&alloc, req);
				more = _bulk;
			}
		} catch (...) { }
	}

	void _finish()
	{
		_end_time = _timer->elapsed_ms();

		Test_base::_finish();
	}

	void _handle_ack()
	{
		if (_finished) { return; }

		while (_block->tx()->ack_avail()) {

			Block::Packet_descriptor p = _block->tx()->get_acked_packet();
			if (!p.succeeded()) {
				Genode::error("packet failed lba: ", p.block_number(),
				              " count: ", p.block_count());

				if (_stop_on_error) { throw Test_failed(); }
				else                { _finish(); }
			} else {

				/* simulate read */
				if (p.operation() == Block::Packet_descriptor::READ) {
					char * const content = _block->tx()->packet_content(p);
					Genode::memcpy(_scratch_buffer, content, p.size());
				}

				size_t const psize = p.size();
				size_t const count = psize / _block_size;

				_rx += (p.operation() == Block::Packet_descriptor::READ)  * count;
				_tx += (p.operation() == Block::Packet_descriptor::WRITE) * count;

				_bytes += psize;

				if (--request_num == 0) {
					_success = true;
					_finish();
				}
			}

			_block->tx()->release_packet(p);
		}

		_handle_submit();
	}

	Replay(Genode::Env &env, Genode::Allocator &alloc,
	       Genode::Xml_node config,
	       Genode::Signal_context_capability finished_sig)
	: Test_base(finished_sig), env(env), alloc(alloc)
	{
		_verbose = config.attribute_value<bool>("verbose", false);
		_bulk    = config.attribute_value<bool>("bulk", false);

		try {
			config.for_each_sub_node("request", [&](Xml_node request) {
				Block::Packet_descriptor::Opcode op;
				Block::sector_t nr { 0 };
				Genode::size_t  count { 0 };

				try {
					request.attribute("lba").value(&nr);
					request.attribute("count").value(&count);

					Genode::String<8> tmp;
					request.attribute("type").value(&tmp);
					if      (tmp == "read")  { op = Block::Packet_descriptor::READ; }
					else if (tmp == "write") { op = Block::Packet_descriptor::WRITE; }
					else { throw -1; }

					Request *req = new (&alloc) Request(op, nr, count);
					requests.enqueue(req);
					++request_num;
				} catch (...) { return; }
			});
		} catch (...) {
			Genode::error("could not read request list");
			return;
		}
	}

	/********************
	 ** Test interface **
	 ********************/

	void start(bool stop_on_error)  override
	{
		_stop_on_error = stop_on_error;

		_block.construct(env, &_block_alloc, TX_BUF_SIZE);

		_block->tx_channel()->sigh_ack_avail(_ack_sigh);
		_block->tx_channel()->sigh_ready_to_submit(_submit_sigh);

		_block->info(&_block_count, &_block_size, &_block_ops);

		_timer.construct(env);

		_start_time = _timer->elapsed_ms();

		_handle_submit();
	}

	Result finish() override
	{
		_timer.destruct();
		_block.destruct();

		return Result(_success, _end_time - _start_time,
		              _bytes, _rx, _tx, 0u, _block_size);
	}

	char const *name() const { return "replay"; }
};



#endif /* _TEST_REPLAY_H_ */
