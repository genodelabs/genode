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

/* Genode includes */
#include <base/allocator_avl.h>
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/heap.h>
#include <base/log.h>
#include <block_session/connection.h>
#include <os/reporter.h>
#include <timer_session/connection.h>


namespace Test {

	using namespace Genode;

	struct Scratch_buffer;
	struct Result;
	struct Test_base;
	struct Main;

	struct Test_failed              : Exception { };
	struct Constructing_test_failed : Exception { };
}


class Test::Scratch_buffer
{
	private:

		Allocator &_alloc;

		Scratch_buffer(Scratch_buffer const &);
		Scratch_buffer &operator = (Scratch_buffer const&);

	public:

		char   * const base;
		size_t   const size;

		Scratch_buffer (Allocator &alloc, size_t size)
		: _alloc(alloc), base((char*)alloc.alloc(size)), size(size) { }

		~Scratch_buffer() { destroy(&_alloc, base); }
};


struct Test::Result
{
	uint64_t duration     { 0 };
	uint64_t bytes        { 0 };
	uint64_t rx           { 0 };
	uint64_t tx           { 0 };
	uint64_t request_size { 0 };
	uint64_t block_size   { 0 };
	size_t   triggered    { 0 };
	bool     success      { false };

	bool  calculate { false };
	float mibs      { 0.0f };
	float iops      { 0.0f };

	Result() { }

	Result(bool success, uint64_t d, uint64_t b, uint64_t rx, uint64_t tx,
	       uint64_t rsize, uint64_t bsize, size_t triggered)
	:
		duration(d), bytes(b), rx(rx), tx(tx),
		request_size(rsize ? rsize : bsize), block_size(bsize),
		triggered(triggered), success(success)
	{
		mibs = ((double)bytes / ((double)duration/1000)) / (1024 * 1024);
		/* total ops / seconds w/o any latency inclusion */
		iops = (double)((rx + tx) / (request_size / block_size))
		     / ((double)duration / 1000);
	}

	void print(Genode::Output &out) const
	{
		Genode::print(out, "rx:",       rx,           " ",
		                   "tx:",       tx,           " ",
		                   "bytes:",    bytes,        " ",
		                   "size:",     request_size, " ",
		                   "bsize:",    block_size,   " ",
		                   "duration:", duration,     " "
		);

		if (calculate) {
			Genode::print(out, "mibs:", mibs, " iops:", iops);
		}

		Genode::print(out, " triggered:", triggered);
		Genode::print(out, " result:", success ? "ok" : "failed");
	}
};


/*
 * Base class used for test running list
 */
struct Test::Test_base : private Genode::Fifo<Test_base>::Element
{
	protected:

		Env       &_env;
		Allocator &_alloc;

		Xml_node const _node;

		typedef Block::block_number_t block_number_t;

		bool     const _verbose;
		size_t   const _io_buffer;
		uint64_t const _progress_interval;
		bool     const _copy;
		size_t   const _batch;

		Constructible<Timer::Connection> _timer { };

		Constructible<Timer::Periodic_timeout<Test_base>> _progress_timeout { };

		Allocator_avl _block_alloc { &_alloc };

		struct Job;
		typedef Block::Connection<Job> Block_connection;

		Constructible<Block_connection> _block { };

		struct Job : Block_connection::Job
		{
			unsigned const id;

			Job(Block_connection &connection, Block::Operation operation, unsigned id)
			:
				Block_connection::Job(connection, operation), id(id)
			{ }
		};

		/*
		 * Must be called by every test when it has finished
		 */
		Genode::Signal_context_capability _finished_sig;

		void finish()
		{
			_end_time = _timer->elapsed_ms();

			_finished = true;
			if (_finished_sig.valid()) {
				Genode::Signal_transmitter(_finished_sig).submit();
			}

			_timer.destruct();
		}

		Block::Session::Info _info { };

		size_t _length_in_blocks { 0 };
		size_t _size_in_blocks   { 0 };

		uint64_t _start_time { 0 };
		uint64_t _end_time   { 0 };
		size_t   _bytes      { 0 };
		uint64_t _rx         { 0 };
		uint64_t _tx         { 0 };
		size_t   _triggered  { 0 };  /* number of I/O signals */
		unsigned _job_cnt    { 0 };
		unsigned _completed  { 0 };

		bool _stop_on_error { true };
		bool _finished      { false };
		bool _success       { false };

		Scratch_buffer &_scratch_buffer;

		void _memcpy(char *dst, char const *src, size_t length)
		{
			if (length > _scratch_buffer.size) {
				warning("scratch buffer too small for copying");
				return;
			}

			Genode::memcpy(dst, src, length);
		}

	public:

		/**
		 * Block::Connection::Update_jobs_policy
		 */
		void produce_write_content(Job &job, off_t offset, char *dst, size_t length)
		{
			_tx    += length / _info.block_size;
			_bytes += length;

			if (_verbose)
				log("job ", job.id, ": writing ", length, " bytes at ", offset);

			if (_copy)
				_memcpy(dst, _scratch_buffer.base, length);
		}

		/**
		 * Block::Connection::Update_jobs_policy
		 */
		void consume_read_result(Job &job, off_t offset,
		                         char const *src, size_t length)
		{
			_rx    += length / _info.block_size;
			_bytes += length;

			if (_verbose)
				log("job ", job.id, ": got ", length, " bytes at ", offset);

			if (_copy)
				_memcpy(_scratch_buffer.base, src, length);
		}

		/**
		 * Block_connection::Update_jobs_policy
		 */
		void completed(Job &job, bool success)
		{
			_completed++;

			if (_verbose)
				log("job ", job.id, ": ", job.operation(), ", completed");

			if (!success)
				error("processing ", job.operation(), " failed");

			destroy(_alloc, &job);

			if (!success && _stop_on_error)
				throw Test_failed();

			/* replace completed job by new one */
			_spawn_job();

			bool const jobs_active = (_job_cnt != _completed);

			_success = !jobs_active && success;

			if (!jobs_active || !success)
				finish();
		}

	protected:

		void _handle_progress_timeout(Duration)
		{
			log("progress: rx:", _rx, " tx:", _tx);
		}

		void _handle_block_io()
		{
			_triggered++;
			_block->update_jobs(*this);
		}

		Signal_handler<Test_base> _block_io_sigh {
			_env.ep(), *this, &Test_base::_handle_block_io };

	public:

		friend class Genode::Fifo<Test_base>;

		Test_base(Env &env, Allocator &alloc, Xml_node node,
		          Signal_context_capability finished_sig,
		          Scratch_buffer &scratch_buffer)
		:
			_env(env), _alloc(alloc), _node(node),
			_verbose(node.attribute_value("verbose", false)),
			_io_buffer(_node.attribute_value("io_buffer",
			                                 Number_of_bytes(4*1024*1024))),
			_progress_interval(_node.attribute_value("progress", (uint64_t)0)),
			_copy(_node.attribute_value("copy", true)),
			_batch(_node.attribute_value("batch", 1u)),
			_finished_sig(finished_sig),
			_scratch_buffer(scratch_buffer)
		{
			if (_progress_interval)
				_progress_timeout.construct(*_timer, *this,
				                            &Test_base::_handle_progress_timeout,
				                            Microseconds(_progress_interval*1000));
		}

		virtual ~Test_base() { };

		void start(bool stop_on_error)
		{
			_stop_on_error = stop_on_error;

			_block.construct(_env, &_block_alloc, _io_buffer);
			_block->sigh(_block_io_sigh);
			_info = _block->info();

			_init();

			for (unsigned i = 0; i < _batch; i++)
				_spawn_job();

			_timer.construct(_env);
			_start_time = _timer->elapsed_ms();

			_handle_block_io();
		}

		/********************
		 ** Test interface **
		 ********************/

		virtual void _init() = 0;
		virtual void _spawn_job() = 0;
		virtual Result result() = 0;
		virtual char const *name() const = 0;
		virtual void print(Output &) const = 0;
};


/* tests */
#include <test_ping_pong.h>
#include <test_random.h>
#include <test_replay.h>
#include <test_sequential.h>


/*
 * Main
 */
struct Test::Main
{
	Genode::Env  &_env;
	Genode::Heap  _heap { _env.ram(), _env.rm() };

	Genode::Attached_rom_dataspace _config_rom { _env, "config" };

	bool const _log {
		_config_rom.xml().attribute_value("log", false) };

	bool const _report {
		_config_rom.xml().attribute_value("report", false) };

	bool const _calculate {
		_config_rom.xml().attribute_value("calculate", true) };

	bool const _stop_on_error {
		_config_rom.xml().attribute_value("stop_on_error", true) };

	Genode::Number_of_bytes const _scratch_buffer_size {
		_config_rom.xml().attribute_value("scratch_buffer_size",
		                                  Genode::Number_of_bytes(1U<<20)) };

	Genode::Fifo<Test_base> _tests { };

	struct Test_result : Genode::Fifo<Test_result>::Element
	{
		Genode::String<32> name { };
		Test::Result result { };

		Test_result(char const *name) : name(name) { };
	};
	Genode::Fifo<Test_result> _results { };

	Genode::Reporter _result_reporter { _env, "results" };

	void _generate_report()
	{
		try {
			Genode::Reporter::Xml_generator xml(_result_reporter, [&] () {
				_results.for_each([&] (Test_result &tr) {
					xml.node("result", [&] () {
						xml.attribute("test",     tr.name);
						xml.attribute("rx",       tr.result.rx);
						xml.attribute("tx",       tr.result.tx);
						xml.attribute("bytes",    tr.result.bytes);
						xml.attribute("size",     tr.result.request_size);
						xml.attribute("bsize",    tr.result.block_size);
						xml.attribute("duration", tr.result.duration);

						if (_calculate) {
							/* XXX */
							xml.attribute("mibs", (unsigned)(tr.result.mibs * (1<<20u)));
							xml.attribute("iops", (unsigned)(tr.result.iops + 0.5f));
						}

						xml.attribute("result", tr.result.success ? 0 : 1);
					});
				});
			});
		} catch (...) { Genode::warning("could generate results report"); }
	}

	Test_base *_current { nullptr };

	bool _success { true };

	void _handle_finished()
	{
		/* clean up current test */
		if (_current) {
			Result r = _current->result();

			if (!r.success) { _success = false; }

			r.calculate = _calculate;

			if (_log) {
				Genode::log("finished ", _current->name(), " ", r);
			}

			if (_report) {
				Test_result *tr = new (&_heap) Test_result(_current->name());
				tr->result = r;
				_results.enqueue(*tr);

				_generate_report();
			}

			Genode::destroy(&_heap, _current);
			_current = nullptr;
		}

		/* execute next test */
		if (!_current) {
			_tests.dequeue([&] (Test_base &head) {
				if (_log) { Genode::log("start ", head); }
				try {
					head.start(_stop_on_error);
					_current = &head;
				} catch (...) {
					Genode::log("Could not start ", head);
					Genode::destroy(&_heap, &head);
					throw;
				}
			});
		}

		if (!_current) {
			/* execution is finished */
			Genode::log("--- all tests finished ---");
			_env.parent().exit(_success ? 0 : 1);
		}
	}

	Genode::Signal_handler<Main> _finished_sigh {
		_env.ep(), *this, &Main::_handle_finished };

	Scratch_buffer _scratch_buffer { _heap, _scratch_buffer_size };

	void _construct_tests(Genode::Xml_node config)
	{
		try {
			Genode::Xml_node tests = config.sub_node("tests");
			tests.for_each_sub_node([&] (Genode::Xml_node node) {

				if (node.has_type("ping_pong")) {
					Test_base *t = new (&_heap)
						Ping_pong(_env, _heap, node, _finished_sigh, _scratch_buffer);
					_tests.enqueue(*t);
				} else

				if (node.has_type("random")) {
					Test_base *t = new (&_heap)
						Random(_env, _heap, node, _finished_sigh, _scratch_buffer);
					_tests.enqueue(*t);
				} else

				if (node.has_type("replay")) {
					Test_base *t = new (&_heap)
						Replay(_env, _heap, node, _finished_sigh, _scratch_buffer);
					_tests.enqueue(*t);
				} else

				if (node.has_type("sequential")) {
					Test_base *t = new (&_heap)
						Sequential(_env, _heap, node, _finished_sigh, _scratch_buffer);
					_tests.enqueue(*t);
				}
			});
		} catch (...) { Genode::error("invalid tests"); }
	}

	/**
	 * Constructor
	 */
	Main(Genode::Env &env) : _env(env)
	{
		_result_reporter.enabled(_report);

		try {
			_construct_tests(_config_rom.xml());
		} catch (...) { throw; }

		Genode::log("--- start tests ---");

		/* initial kick-off */
		_handle_finished();
	}

	~Main()
	{
		_results.dequeue_all([&] (Test_result &tr) {
			Genode::destroy(&_heap, &tr); });
	}

	private:

		Main(const Main&) = delete;
		Main& operator=(const Main&) = delete;
};


void Component::construct(Genode::Env &env)
{
	static Test::Main main(env);
}
