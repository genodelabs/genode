/*
 * \brief  Block session testing
 * \author Josef Soentgen
 * \author Norman Feske
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

/* local includes */
#include <test_ping_pong.h>
#include <test_random.h>
#include <test_replay.h>
#include <test_sequential.h>

namespace Test {
	struct Config;
	struct Scratch_buffer;
	struct Result;
	struct Test_job;
	struct Block_connection;
	struct Test;
	struct Main;
}


struct Test::Config
{
	bool stop_on_error, log, report, calculate;
	size_t scratch_buffer_size;

	static Config from_xml(Xml_node const &node)
	{
		return {
			.stop_on_error = node.attribute_value("stop_on_error", true),
			.log           = node.attribute_value("log",           false),
			.report        = node.attribute_value("report",        false),
			.calculate     = node.attribute_value("calculate",     true),
			.scratch_buffer_size = node.attribute_value("scratch_buffer_size",
			                                            Number_of_bytes(1U<<20)),
		};
	}
};


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
	bool     success;
	uint64_t duration;
	Total    total;
	Total    rx;
	Total    tx;
	size_t   request_size;
	size_t   block_size;
	size_t   triggered;

	double mibs() const
	{
		if (!duration)
			return 0;

		return ((double)total.bytes / ((double)duration/1000)) / (1024 * 1024);
	}

	double iops() const
	{
		if (!duration || !request_size)
			return 0;

		return (double)((rx.bytes + tx.bytes) / (request_size / block_size))
		     / ((double)duration / 1000);
	}

	void print(Output &out) const
	{
		using Genode::print;

		print(out, "rx:",        rx,    " "
		           "tx:",        tx,    " "
		           "bytes:",     total, " "
		           "size:",      Number_of_bytes(request_size), " "
		           "bsize:",     Number_of_bytes(block_size),   " "
		           "mibs:",      mibs(),    " "
		           "iops:",      iops(),    " "
		           "duration:",  duration,  " "
		           "triggered:", triggered, " "
		           "result:",    success ? "ok" : "failed");
	}
};


struct Test::Test_job : Block::Connection<Test_job>::Job
{
	unsigned const id;

	Test_job(Block::Connection<Test_job> &conn, Block::Operation op, unsigned id)
	:
		Block::Connection<Test_job>::Job(conn, op), id(id)
	{ }
};


struct Test::Block_connection : Block::Connection<Test_job>
{
	size_t const block_size = info().block_size;

	Config const _config;

	struct Attr { bool copy, verbose; };

	Attr const _attr;

	Allocator &_alloc;

	Stats stats { };

	struct Action : Genode::Interface
	{
		virtual void spawn_jobs(Stats &) = 0;
		virtual void job_failed() = 0;
		virtual void all_jobs_completed() = 0;
	};

	Action &_action;

	Scratch_buffer &_scratch_buffer;

	Block_connection(Config config, Attr attr, Allocator &alloc, Action &action,
	                 Scratch_buffer &scratch_buffer, auto &&... args)
	:
		Block::Connection<Test_job>(args...),
		_config(config), _attr(attr), _alloc(alloc), _action(action),
		_scratch_buffer(scratch_buffer)
	{ }

	void _memcpy(char *dst, char const *src, size_t length)
	{
		if (length > _scratch_buffer.size) {
			warning("scratch buffer too small for copying");
			return;
		}

		Genode::memcpy(dst, src, length);
	}

	/**
	 * Block::Connection::Update_jobs_policy
	 */
	void produce_write_content(Test_job &job, Block::seek_off_t offset, char *dst, size_t length)
	{
		stats.tx.bytes    += length / block_size;
		stats.total.bytes += length;

		if (_attr.verbose)
			log("job ", job.id, ": writing ", length, " bytes at ", offset);

		if (_attr.copy)
			_memcpy(dst, _scratch_buffer.base, length);
	}

	/**
	 * Block::Connection::Update_jobs_policy
	 */
	void consume_read_result(Test_job &job, Block::seek_off_t offset,
	                         char const *src, size_t length)
	{
		stats.rx.bytes    += length / block_size;
		stats.total.bytes += length;

		if (_attr.verbose)
			log("job ", job.id, ": got ", length, " bytes at ", offset);

		if (_attr.copy)
			_memcpy(_scratch_buffer.base, src, length);
	}

	/**
	 * Block_connection::Update_jobs_policy
	 */
	void completed(Test_job &job, bool success)
	{
		stats.completed++;

		if (_attr.verbose)
			log("job ", job.id, ": ", job.operation(), ", completed");

		if (!success) {
			error("processing ", job.operation(), " failed");
			for (;;);
		}

		destroy(_alloc, &job);

		/* replace completed job by new one */
		_action.spawn_jobs(stats);

		if (!success)
			_action.job_failed();

		if (stats.job_cnt == stats.completed)
			_action.all_jobs_completed();
	}

	void print(Output &out) const
	{
		Genode::print(out, "rx:", stats.rx, " tx:", stats.tx);
	}
};


/*
 * Mechanism for executing a test scenario
 */
struct Test::Test
{
	private:

		Env       &_env;
		Allocator &_alloc;
		Scenario  &_scenario;

		Timer::Connection _timer { _env };

		Constructible<Timer::Periodic_timeout<Test>> _progress_timeout { };

		Allocator_avl _block_alloc { &_alloc };

		Block_connection _block;

		/*
		 * Must be called by every test when it has finished
		 */
		Signal_context_capability _finished_sig;

		void finish()
		{
			_end_time = _timer.elapsed_ms();

			_finished = true;
			if (_finished_sig.valid())
				Signal_transmitter(_finished_sig).submit();
		}

		uint64_t const _start_time = _timer.elapsed_ms();

		uint64_t _end_time   { 0 };
		size_t   _triggered  { 0 };  /* number of I/O signals */

		bool _finished { false };
		bool _success  { false };

		Scratch_buffer &_scratch_buffer;

		void _handle_progress_timeout(Duration)
		{
			log("progress: ", _block);
		}

		void _handle_block_io()
		{
			_triggered++;
			_block.update_jobs(_block);
		}

		Signal_handler<Test> _block_io_sigh {
			_env.ep(), *this, &Test::_handle_block_io };

		struct Block_action : Block_connection::Action
		{
			Test &_test;

			Block_action(Test &test) : _test(test) { }

			void spawn_jobs(Stats &stats) override
			{
				for (;;) {
					unsigned const active_jobs = stats.job_cnt - stats.completed;
					if (active_jobs >= _test._scenario.attr.batch)
						break;

					bool const job_spawned =
						_test._scenario.next_job(stats).convert<bool>(
							[&] (Block::Operation operation) {
								stats.job_cnt++;
								new (_test._alloc)
									Test_job(_test._block, operation, stats.job_cnt);
								return true;
							},
							[&] (Scenario::No_job) {
								return false; });

					if (!job_spawned)
						break;
				}
			}

			void job_failed() override
			{
				_test.finish();
				_test._success = false;
			}

			void all_jobs_completed() override
			{
				_test.finish();
				_test._success = true;
			}

		} _block_action { *this };

	public:

		Test(Env &env, Allocator &alloc, Config const &config, Scenario &scenario,
		     Signal_context_capability finished_sig,
		     Scratch_buffer &scratch_buffer)
		:
			_env(env), _alloc(alloc), _scenario(scenario),
			_block(config, { .copy    = _scenario.attr.copy,
			                 .verbose = _scenario.attr.verbose },
			       _alloc, _block_action, scratch_buffer,
			       _env, &_block_alloc, _scenario.attr.io_buffer),
			_finished_sig(finished_sig),
			_scratch_buffer(scratch_buffer)
		{
			if (_scenario.attr.progress_interval)
				_progress_timeout.construct(_timer, *this,
				                            &Test::_handle_progress_timeout,
				                            Microseconds(_scenario.attr.progress_interval*1000));

			_block.sigh(_block_io_sigh);

			bool const init_ok =
				_scenario.init({ .block_size          = _block.info().block_size,
				                 .block_count         = _block.info().block_count,
				                 .scratch_buffer_size = _scratch_buffer.size });

			if (!init_ok) {
				error("initialization of ", scenario.name(), " failed");
				return;
			}

			_block_action.spawn_jobs(_block.stats);

			_handle_block_io();
		}

		char const *name() const { return _scenario.name(); }

		Result result()
		{
			return {
				.success      = _success,
				.duration     = _end_time - _start_time,
				.total        = _block.stats.total,
				.rx           = _block.stats.rx,
				.tx           = _block.stats.tx,
				.request_size = _scenario.request_size(),
				.block_size   = _block.block_size,
				.triggered    = _triggered
			};
		}
};


struct Test::Main
{
	Env  &_env;
	Heap  _heap { _env.ram(), _env.rm() };

	Attached_rom_dataspace _config_rom { _env, "config" };

	Config const _config = Config::from_xml(_config_rom.xml());

	Fifo<Scenario> _scenarios { };

	struct Test_result : Fifo<Test_result>::Element
	{
		String<32> name { };
		Result result { };

		Test_result(char const *name) : name(name) { };
	};

	Fifo<Test_result> _results { };

	Constructible<Expanding_reporter> _result_reporter { };

	void _generate_report()
	{
		if (!_result_reporter.constructed())
			return;

		_result_reporter->generate([&] (Xml_generator &xml) {
			_results.for_each([&] (Test_result &tr) {
				xml.node("result", [&] () {
					xml.attribute("test",     tr.name);
					xml.attribute("rx",       tr.result.rx.bytes);
					xml.attribute("tx",       tr.result.tx.bytes);
					xml.attribute("bytes",    tr.result.total.bytes);
					xml.attribute("size",     tr.result.request_size);
					xml.attribute("bsize",    tr.result.block_size);
					xml.attribute("duration", tr.result.duration);

					xml.attribute("mibs", (unsigned)(tr.result.mibs() * (1<<20u)));
					xml.attribute("iops", (unsigned)(tr.result.iops() + 0.5f));

					xml.attribute("result", tr.result.success ? 0 : 1);
				});
			});
		});
	}

	Scenario *_current_ptr = nullptr;

	Constructible<Test> _test { };

	bool _overall_success = true;

	void _handle_finished()
	{
		/* clean up current test */
		if (_current_ptr && _test.constructed()) {
			Result r = _test->result();

			if (!r.success)
				_overall_success = false;

			if (_config.log)
				log("finished ", _current_ptr->name(), " ", r);

			if (_config.report) {
				Test_result *tr = new (&_heap) Test_result(_current_ptr->name());
				tr->result = r;
				_results.enqueue(*tr);

				_generate_report();
			}

			_test.destruct();
			destroy(&_heap, _current_ptr);
			_current_ptr = nullptr;
		}

		/* execute next test */
		if (_overall_success || !_config.stop_on_error) {
			_scenarios.dequeue([&] (Scenario &head) {
				if (_config.log) { log("start ", head); }
				try {
					_test.construct(_env, _heap, _config, head,
					                _finished_sigh, _scratch_buffer);
					_current_ptr = &head;
				} catch (...) {
					log("Could not start ", head);
					destroy(&_heap, &head);
					throw;
				}
			});
		}

		if (!_current_ptr) {
			/* execution is finished */
			log("--- all tests finished ---");
			_env.parent().exit(_overall_success ? 0 : 1);
		}
	}

	Signal_handler<Main> _finished_sigh {
		_env.ep(), *this, &Main::_handle_finished };

	Scratch_buffer _scratch_buffer { _heap, _config.scratch_buffer_size };

	void _construct_scenarios(Xml_node const &config)
	{
		auto create = [&] (Xml_node const &node) -> Scenario *
		{
			if (node.has_type("ping_pong"))  return new (&_heap) Ping_pong (_heap, node);
			if (node.has_type("random"))     return new (&_heap) Random    (_heap, node);
			if (node.has_type("replay"))     return new (&_heap) Replay    (_heap, node);
			if (node.has_type("sequential")) return new (&_heap) Sequential(_heap, node);
			return nullptr;
		};

		try {
			config.with_sub_node("tests",
				[&] (Xml_node const &tests) {
					tests.for_each_sub_node([&] (Xml_node const &node) {
						Scenario *ptr = create(node);
						if (ptr)
							_scenarios.enqueue(*ptr); });
				},
				[&] {
					error("config lacks <tests> sub node");
				});
		} catch (...) { error("invalid tests"); }
	}

	Main(Env &env) : _env(env)
	{
		_result_reporter.conditional(_config.report, env, "results");

		try {
			_construct_scenarios(_config_rom.xml());
		} catch (...) { throw; }

		log("--- start tests ---");

		/* initial kick-off */
		_handle_finished();
	}

	~Main()
	{
		_results.dequeue_all([&] (Test_result &tr) {
			destroy(&_heap, &tr); });
	}

	private:

		Main(const Main&) = delete;
		Main& operator=(const Main&) = delete;
};


void Component::construct(Genode::Env &env)
{
	static Test::Main main(env);
}
