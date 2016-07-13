#include <base/allocator_avl.h>
#include <block_session/connection.h>
#include <os/server.h>
#include <timer_session/connection.h>

#include <stdio.h>

using namespace Genode;

enum {
	TEST_WRITE   = false,
	TEST_SIZE    = 1024 * 1024 * 1024,
	REQUEST_SIZE = 8 * 512,
	TX_BUFFER    = Block::Session::TX_QUEUE_SIZE * REQUEST_SIZE
};


namespace Test {
	class Throughput;
	struct Main;
}


class Test::Throughput
{
	private:

		Allocator_avl     _alloc{env()->heap() };
		Block::Connection _session { &_alloc, TX_BUFFER };
		Timer::Connection _timer;

		Signal_rpc_member<Throughput> _disp_ack;
		Signal_rpc_member<Throughput> _disp_submit;
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
			static size_t count            = REQUEST_SIZE / _blk_size;

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

		void _ready_to_submit(unsigned)
		{
			_submit();
		}

		void _ack_avail(unsigned)
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
			::printf("%s %zu KB in %lu ms (%.02f MB/s)\n",
			         !_read_done ? "Read" : "Wrote",
			         _bytes / 1024, _stop - _start,
			         ((double)_bytes / (1024 * 1024)) / ((double)(_stop - _start) / 1000));


			/* start write */
			if (!_read_done ) {
				_read_done  = true;
				_start      = _timer.elapsed_ms();
				_bytes      = 0;
				_current    = 0;
				if (TEST_WRITE)
					_submit();
				else
					::printf("Done\n");
			} else if (!_write_done && TEST_WRITE) {
				_write_done = true;
				::printf("Done\n");
			}
		}

	public:

		Throughput(Server::Entrypoint &ep)
		: _disp_ack(ep, *this, &Throughput::_ack_avail),
		  _disp_submit(ep, *this, &Throughput::_ready_to_submit)
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


struct Test::Main
{
	Main(Server::Entrypoint &ep)
	{
		new (env()->heap()) Throughput(ep);
	}
};


namespace Server {
	char const *name()       { return "block_bench_ep"; };
	size_t      stack_size() { return 2*1024*sizeof(long); }

	void construct(Entrypoint &ep)
	{
		static Test::Main server(ep);
	}
}
