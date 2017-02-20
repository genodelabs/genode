/*
 * \brief  Benchmark for block connection
 * \author Sebastian Sumpf
 * \author Stefan Kalkowski
 * \date   2015-03-24
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/allocator_avl.h>
#include <base/component.h>
#include <base/heap.h>
#include <base/log.h>
#include <block_session/connection.h>
#include <timer_session/connection.h>

using namespace Genode;

enum {
	TEST_WRITE   = false,
	TEST_SIZE    = 1024 * 1024 * 1024,
	REQUEST_SIZE = 8 * 512,
	TX_BUFFER    = Block::Session::TX_QUEUE_SIZE * REQUEST_SIZE
};


class Throughput
{
	private:

		typedef Genode::size_t size_t;

		Env &             _env;
		Heap              _heap    { _env.ram(), _env.rm() };
		Allocator_avl     _alloc   { &_heap };
		Block::Connection _session { _env, &_alloc, TX_BUFFER };
		Timer::Connection _timer   { _env };

		Signal_handler<Throughput> _disp_ack    { _env.ep(), *this,
		                                          &Throughput::_ack };
		Signal_handler<Throughput> _disp_submit { _env.ep(), *this,
		                                          &Throughput::_submit };
		bool                          _read_done  = false;
		bool                          _write_done = false;

		unsigned long   _start = 0;
		unsigned long   _stop  = 0;
		size_t          _bytes = 0;
		Block::sector_t _current = 0;

		size_t          _blk_size;
		Block::sector_t _blk_count;

		void _submit()
		{
			static size_t count = REQUEST_SIZE / _blk_size;

			if (_read_done && (_write_done || !TEST_WRITE))
				return;

			try {
				while (_session.tx()->ready_to_submit()) {
					Block::Packet_descriptor p(
						_session.tx()->alloc_packet(REQUEST_SIZE),
						!_read_done ? Block::Packet_descriptor::READ : Block::Packet_descriptor::WRITE,
						_current, count);

					_session.tx()->submit_packet(p);

					/* increment for next read */
					_current += count;
					if (_current + count >= _blk_count)
						_current = 0;
				}
			} catch (...) { }
		}

		void _ack()
		{
			while (_session.tx()->ack_avail()) {

				Block::Packet_descriptor p = _session.tx()->get_acked_packet();
				if (!p.succeeded())
					error("packet error: block: ", p.block_number(), " "
					      "count: ", p.block_count());

				if (!_read_done || (_read_done &&  p.operation() == Block::Packet_descriptor::WRITE))
					_bytes += p.size();

				_session.tx()->release_packet(p);
			}

			if (_bytes >= TEST_SIZE) {
				_finish();
				return;
			}

			_submit();
		}

		void _finish()
		{
			if (_read_done && (_write_done || !TEST_WRITE))
				return;

			_stop = _timer.elapsed_ms();
			log(!_read_done ? "Read" : "Wrote", " ", _bytes / 1024, " KB in ",
			    _stop - _start, " ms (",
				((double)_bytes / (1024 * 1024)) / ((double)(_stop - _start) / 1000),
				" MB/s)");

			/* start write */
			if (!_read_done ) {
				_read_done  = true;
				_start      = _timer.elapsed_ms();
				_bytes      = 0;
				_current    = 0;
				if (TEST_WRITE)
					_submit();
				else
					log("Done");
			} else if (!_write_done && TEST_WRITE) {
				_write_done = true;
				log("Done");
			}
		}

	public:

		Throughput(Env & env)
		: _env(env)
		{
			_session.tx_channel()->sigh_ack_avail(_disp_ack);
			_session.tx_channel()->sigh_ready_to_submit(_disp_submit);

			Block::Session::Operations blk_ops;
			_session.info(&_blk_count, &_blk_size, &blk_ops);

			warning("block count ", _blk_count, " size ", _blk_size);
			log("read/write ", TEST_SIZE / 1024, " KB ...");
			_start = _timer.elapsed_ms();
			_submit();
		}
};


void Component::construct(Env &env) { static Throughput test(env); }
