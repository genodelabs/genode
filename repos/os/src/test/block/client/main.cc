/*
 * \brief  Block session tests - client side.
 * \author Stefan Kalkowski
 * \date   2013-12-10
 */

#include <base/allocator_avl.h>
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/heap.h>
#include <base/log.h>
#include <block_session/connection.h>
#include <timer_session/connection.h>
#include <os/ring_buffer.h>

static Genode::size_t  blk_sz;     /* block size of the device   */
static Block::sector_t test_cnt;   /* number test blocks         */
static Block::sector_t blk_cnt;    /* number of blocks of device */
static bool            writeable;


/**
 * Virtual base class of all test scenarios, provides basic signal handling
 */
class Test : Genode::Interface
{
	public:

		struct Exception : private Genode::Exception, Genode::Interface
		{
			virtual void print_error() = 0;
		};

		class Block_exception : public Exception
		{
			protected:

				Block::sector_t _nr;
				Genode::size_t  _cnt;
				bool            _write;

			public:

				Block_exception(Block::sector_t nr, Genode::size_t cnt,
				                bool write)
				: _nr(nr), _cnt(cnt), _write(write) {}

				void print_error() override
				{
					Genode::error("couldn't ", _write ? "write" : "read", " "
					              "block ", _nr, " - ", _nr+_cnt);
				}
		};

		struct Submit_queue_full : Exception {
			void print_error() override {
				Genode::error("submit queue is full!"); } };

		struct Timeout : Exception {
			void print_error() override {
				Genode::error("test timed out!"); } };

		virtual void perform()         = 0;
		virtual void ack_avail()       = 0;

	protected:

		Genode::Entrypoint   &_ep;
		Genode::Allocator_avl _alloc;
		Block::Connection<>   _session;

		Genode::Io_signal_handler<Test> _disp_ack;
		Genode::Io_signal_handler<Test> _disp_submit;
		Genode::Io_signal_handler<Test> _disp_timeout;

		Timer::Connection _timer;
		bool              _handle = false;

		Genode::size_t _shared_buffer_size(Genode::size_t bulk)
		{
			return bulk +
			       sizeof(Block::Session::Tx_policy::Ack_queue) +
			       sizeof(Block::Session::Tx_policy::Submit_queue) +
			       (1 << Block::Packet_descriptor::PACKET_ALIGNMENT) - 1;
		}

		void _ack_avail()       { ack_avail();     }
		void _ready_to_submit() { _handle = false; }
		void _timeout()         { throw Timeout(); }

		Test(Genode::Env  &env,
		     Genode::Heap &heap,
		     Genode::size_t bulk_buffer_size,
		     Genode::uint64_t timeout_ms)
		: _ep(env.ep()),
		  _alloc(&heap),
		  _session(env, &_alloc, _shared_buffer_size(bulk_buffer_size)),
		  _disp_ack(env.ep(), *this, &Test::_ack_avail),
		  _disp_submit(env.ep(), *this, &Test::_ready_to_submit),
		  _disp_timeout(env.ep(), *this, &Test::_timeout),
		  _timer(env)
		{
			_session.tx_channel()->sigh_ack_avail(_disp_ack);
			_session.tx_channel()->sigh_ready_to_submit(_disp_submit);

			if (timeout_ms) {
				_timer.sigh(_disp_timeout);
				_timer.trigger_once(1000*timeout_ms);
			}
		}

		void _handle_signal()
		{
			_handle = true;
			while (_handle) _ep.wait_and_dispatch_one_io_signal();
		}
};


template <unsigned BULK_BLK_NR, unsigned NR_PER_REQ>
struct Read_test : Test
{
	bool done;

	Read_test(Genode::Env &env, Genode::Heap &heap, Genode::uint64_t timeo_ms)
	: Test(env, heap, BULK_BLK_NR*blk_sz, timeo_ms), done(false) { }

	void perform() override
	{
		Genode::log("reading block 0 - ", test_cnt - 1, ", ", NR_PER_REQ,
		            " per request");

		for (Block::sector_t nr = 0, cnt = NR_PER_REQ; nr < test_cnt;
		     nr += cnt) {

			while (!_session.tx()->ready_to_submit())
				_handle_signal();

			cnt = Genode::min<Block::sector_t>(NR_PER_REQ, test_cnt-nr);

			try {
				Block::Packet_descriptor p(
					_session.alloc_packet(cnt*blk_sz),
					Block::Packet_descriptor::READ, nr, cnt);
				_session.tx()->submit_packet(p);
			} catch(Block::Session::Tx::Source::Packet_alloc_failed) {
				cnt = 0; /* retry the current block number */
				_handle_signal();
			}
		}

		while (!done)
			_handle_signal();
	}

	void ack_avail() override
	{
		 _handle = false;

		while (_session.tx()->ack_avail()) {
			Block::Packet_descriptor p = _session.tx()->get_acked_packet();

			if (!p.succeeded())
				throw Block_exception(p.block_number(), p.block_count(),
				                      false);

			if ((p.block_number() + p.block_count()) == test_cnt)
				done = true;

			_session.tx()->release_packet(p);
		}
	}
};


template <unsigned BULK_BLK_NR, unsigned NR_PER_REQ, unsigned BATCH>
struct Write_test : Test
{
	struct Invalid_dimensions : Exception {
		void print_error() override {
			Genode::error("invalid bulk buffer, or batch size!"); } };

	struct Integrity_exception : Block_exception
	{
		Integrity_exception(Block::sector_t nr, Genode::size_t cnt)
		: Block_exception(nr, cnt, false) {}

		void print_error() override
		{
			Genode::error("integrity check failed: block ", _nr, " - ",
			              _nr+_cnt);
		}
	};

	typedef Genode::Ring_buffer<Block::Packet_descriptor, BATCH+1,
	                            Genode::Ring_buffer_unsynchronized> Req_buffer;

	Req_buffer read_packets  { };
	Req_buffer write_packets { };

	Write_test(Genode::Env &env, Genode::Heap &heap, Genode::uint64_t timeo_ms)
	: Test(env, heap, BULK_BLK_NR*blk_sz, timeo_ms)
	{
		if (BULK_BLK_NR < BATCH*NR_PER_REQ ||
		    BATCH > Block::Session::TX_QUEUE_SIZE ||
		    BULK_BLK_NR % BATCH != 0)
			throw Invalid_dimensions();
	}

	bool compare(Block::Packet_descriptor &r, Block::Packet_descriptor &w)
	{
		signed char *dst = (signed char*)_session.tx()->packet_content(w),
		            *src = (signed char*)_session.tx()->packet_content(r);
		for (Genode::size_t i = 0; i < blk_sz; i++)
			if (dst[i] != src[i])
				return false;
		return true;
	}

	void compare()
	{
		while (!read_packets.empty()) {
			Block::Packet_descriptor r = read_packets.get();
			while (true) {
				Block::Packet_descriptor w = write_packets.get();
				if (r.block_number() == w.block_number()) {
					if (!compare(r,w))
						throw Integrity_exception(r.block_number(),
						                          r.block_count());

				_session.tx()->release_packet(w);
					break;
				}
				write_packets.add(w);
			}
			_session.tx()->release_packet(r);
		}
	}

	void write(signed char val)
	{
		while (!read_packets.empty()) {
			Block::Packet_descriptor r = read_packets.get();
			Block::Packet_descriptor w(_session.alloc_packet(r.block_count()*blk_sz),
			                    Block::Packet_descriptor::WRITE,
			                    r.block_number(), r.block_count());
			signed char *dst = (signed char*)_session.tx()->packet_content(w),
			            *src = (signed char*)_session.tx()->packet_content(r);
			for (Genode::size_t i = 0; i < blk_sz; i++)
				dst[i] = src[i] + val;
			_session.tx()->submit_packet(w);
			_session.tx()->release_packet(r);
		}
		while (write_packets.avail_capacity())
		   _handle_signal();
	}

	void read(Block::sector_t start, Block::sector_t end)
	{
		using namespace Block;

		for (sector_t nr = start, cnt = Genode::min(NR_PER_REQ, end - start); nr < end;
		     nr += cnt,
		     cnt = Genode::min<sector_t>(NR_PER_REQ, end-nr)) {
			Block::Packet_descriptor p(_session.alloc_packet(cnt*blk_sz),
			                           Block::Packet_descriptor::READ, nr, cnt);
			_session.tx()->submit_packet(p);
		}
		while (read_packets.avail_capacity())
		   _handle_signal();
	}

	void batch(Block::sector_t start, Block::sector_t end, signed char val)
	{
		read(start, end);
		write(val);
		read(start, end);
		compare();
	}

	void perform() override
	{
		if (!writeable)
			return;

		Genode::log("read/write/compare block 0 - ", test_cnt - 1,
		            ", ", NR_PER_REQ, " per request");

		for (Block::sector_t nr = 0, cnt = BATCH*NR_PER_REQ; nr < test_cnt;
		     nr += cnt,
		     cnt = Genode::min<Block::sector_t>(BATCH*NR_PER_REQ, test_cnt-nr)) {
			batch(nr, nr + cnt, 1);
			batch(nr, nr + cnt, -1);
		}
	}

	void ack_avail() override
	{
		 _handle = false;

		while (_session.tx()->ack_avail()) {
			Block::Packet_descriptor p = _session.tx()->get_acked_packet();
			bool write = p.operation() == Block::Packet_descriptor::WRITE;
			if (!p.succeeded())
				throw Block_exception(p.block_number(), p.block_count(), write);
			if (write)
				write_packets.add(p);
			else
				read_packets.add(p);
		}
	}
};


struct Violation_test : Test
{
	struct Write_on_read_only : Exception {
		void print_error() override {
			Genode::error("write on read-only device succeeded!"); } };

	struct Range_check_failed : Block_exception
	{
		Range_check_failed(Block::sector_t nr, Genode::size_t cnt)
		: Block_exception(nr, cnt, false) {}

		void print_error() override
		{
			Genode::error("range check failed: access to block ", _nr,
			              " - ", _nr+_cnt, " succeeded");
		}
	};

	int p_in_fly;

	Violation_test(Genode::Env &env, Genode::Heap &heap, Genode::uint64_t timeo)
	: Test(env, heap, 20*blk_sz, timeo), p_in_fly(0) {}

	void req(Block::sector_t nr, Genode::size_t cnt, bool write)
	{
		Block::Packet_descriptor p(_session.alloc_packet(blk_sz),
		                           write ? Block::Packet_descriptor::WRITE
		                                 : Block::Packet_descriptor::READ,
		                           nr, cnt);
		_session.tx()->submit_packet(p);
		p_in_fly++;
	}

	void perform() override
	{
		if (!writeable)
			req(0, 1, true);

		req(blk_cnt,   1, false);
		req(blk_cnt-1, 2, false);

		while (p_in_fly > 0)
			_handle_signal();
	}

	void ack_avail() override
	{
		 _handle = false;

		while (_session.tx()->ack_avail()) {
			Block::Packet_descriptor p = _session.tx()->get_acked_packet();
			if (p.succeeded()) {
				if (p.operation() == Block::Packet_descriptor::WRITE)
					throw Write_on_read_only();
				throw Range_check_failed(p.block_number(), p.block_count());
			}
			_session.tx()->release_packet(p);
			p_in_fly--;
		}
	}
};


template <typename TEST>
void perform(Genode::Env &env, Genode::Heap &heap, unsigned timeo_ms = 0)
{
	TEST * test = new (&heap) TEST(env, heap, timeo_ms);
	test->perform();
	destroy(&heap, test);
}


void Component::construct(Genode::Env &env)
{
	using namespace Genode;

	try {

		static Heap heap(env.ram(), env.rm());

		/**
		 * First we ask for the block size of the driver to dimension
		 * the queue size for our tests. Moreover, we implicitely test,
		 * whether closing and opening again works for the driver
		 */
		{
			Allocator_avl       alloc(&heap);
			Block::Connection<> blk(env, &alloc);

			Block::Session::Info const info { blk.info() };

			blk_sz    = info.block_size;
			blk_cnt   = info.block_count;
			writeable = info.writeable;
		}

		try {
			Genode::Number_of_bytes test_size;;
			Genode::Attached_rom_dataspace config { env, "config" };
			config.xml().attribute("test_size").value(test_size);
			test_cnt = Genode::min(test_size / blk_sz, blk_cnt);
		} catch (...) { test_cnt = blk_cnt; }

		/* must be multiple of 16 */
		test_cnt &= ~0xfLLU;

		log("block device with block size ", blk_sz, " sector count ",
			blk_cnt, " (testing ", test_cnt, " sectors)");

		perform<Read_test<Block::Session::TX_QUEUE_SIZE-10,
			Block::Session::TX_QUEUE_SIZE-10> >(env, heap);
		perform<Read_test<Block::Session::TX_QUEUE_SIZE*5, 1> >(env, heap);
		perform<Read_test<Block::Session::TX_QUEUE_SIZE, 1> >(env, heap);
		perform<Write_test<Block::Session::TX_QUEUE_SIZE, 8, 16> >(env, heap);
		perform<Violation_test>(env, heap, 1000);

		log("--- all tests finished ---");
	}
	catch (Genode::Service_denied) {
		error("opening block session was denied!");
	}
	catch (Test::Exception &e) {
		error("test failed!");
		e.print_error();
	}
}

