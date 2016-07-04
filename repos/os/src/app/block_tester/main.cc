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

	/*
	 * Result of a test
	 */
	struct Result
	{
		uint64_t duration     { 0 };
		uint64_t bytes        { 0 };
		uint64_t rx           { 0 };
		uint64_t tx           { 0 };
		uint64_t request_size { 0 };
		uint64_t block_size   { 0 };
		bool     success      { false };

		bool  calculate { false };
		float mibs      { 0.0f };
		float iops      { 0.0f };

		Result() { }

		Result(bool success, uint64_t d, uint64_t b, uint64_t rx, uint64_t tx,
		       uint64_t rsize, uint64_t bsize)
		:
			duration(d), bytes(b), rx(rx), tx(tx),
			request_size(rsize ? rsize : bsize), block_size(bsize),
			success(success)
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
				Genode::print(out, "mibs:", (unsigned)(mibs * (1<<20u)),
				              " ", "iops:", (unsigned)(iops + 0.5f));
			}

			Genode::print(out, " result:", success ? "ok" : "failed");
		}
	};

	/*
	 * Base class used for test running list
	 */
	struct Test_base : private Genode::Fifo<Test_base>::Element
	{
		protected:

			/*
			 * Must be called by every test when it has finished
			 */
			Genode::Signal_context_capability _finished_sig;

			void _finish()
			{
				_finished = true;
				if (_finished_sig.valid()) {
					Genode::Signal_transmitter(_finished_sig).submit();
				}
			}

			bool _verbose { false };

			Block::Session::Operations _block_ops   {   };
			Block::sector_t            _block_count { 0 };
			size_t                     _block_size  { 0 };

			size_t _length_in_blocks { 0 };
			size_t _size_in_blocks   { 0 };

			uint64_t _start_time { 0 };
			uint64_t _end_time   { 0 };
			size_t   _bytes      { 0 };
			uint64_t _rx         { 0 };
			uint64_t _tx         { 0 };

			bool _stop_on_error { true };
			bool _finished      { false };
			bool _success       { false };

		public:

			friend class Genode::Fifo<Test_base>;

			Test_base(Genode::Signal_context_capability finished_sig)
			: _finished_sig(finished_sig) { }

			virtual ~Test_base() { };

			/********************
			 ** Test interface **
			 ********************/

			virtual void start(bool stop_on_error) = 0;
			virtual Result finish() = 0;
			virtual char const *name() const = 0;
	};

	struct Test_failed              : Genode::Exception { };
	struct Constructing_test_failed : Genode::Exception { };

	struct Main;

} /* Test */

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

	bool const _verbose {
		_config_rom.xml().attribute_value("verbose", false) };

	bool const _log {
		_config_rom.xml().attribute_value("log", false) };

	bool const _report {
		_config_rom.xml().attribute_value("report", false) };

	bool const _calculate {
		_config_rom.xml().attribute_value("calculate", true) };

	bool const _stop_on_error {
		_config_rom.xml().attribute_value("stop_on_error", true) };

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
				for (Test_result *tr = _results.head(); tr;
				     tr = tr->next()) {
					xml.node("result", [&] () {
						xml.attribute("test",     tr->name);
						xml.attribute("rx",       tr->result.rx);
						xml.attribute("tx",       tr->result.tx);
						xml.attribute("bytes",    tr->result.bytes);
						xml.attribute("size",     tr->result.request_size);
						xml.attribute("bsize",    tr->result.block_size);
						xml.attribute("duration", tr->result.duration);

						if (_calculate) {
							/* XXX */
							xml.attribute("mibs", (unsigned)(tr->result.mibs * (1<<20u)));
							xml.attribute("iops", (unsigned)(tr->result.iops + 0.5f));
						}

						xml.attribute("result", tr->result.success ? 0 : 1);
					});
				}
			});
		} catch (...) { Genode::warning("could generate results report"); }
	}

	Test_base *_current { nullptr };

	bool _success { true };

	void _handle_finished()
	{
		/* clean up current test */
		if (_current) {
			Result r = _current->finish();

			if (!r.success) { _success = false; }

			r.calculate = _calculate;

			if (_log) {
				Genode::log("test:", _current->name(), " ", r);
			}

			if (_report) {
				Test_result *tr = new (&_heap) Test_result(_current->name());
				tr->result = r;
				_results.enqueue(tr);

				_generate_report();
			}

			if (_verbose) {
				Genode::log("finished ", _current->name(), " ", r.success ? 0 : 1);
			}
			Genode::destroy(&_heap, _current);
			_current = nullptr;
		}

		/* execute next test */
		if (!_current) {
			while (true) {
				_current = _tests.dequeue();
				if (_current) {
					if (_verbose) { Genode::log("start ", _current->name()); }

					try {
						_current->start(_stop_on_error);
						break;
					} catch (...) {
						Genode::log("Could not start ", _current->name());
						Genode::destroy(&_heap, _current);
					}
				} else {
					/* execution is finished */
					Genode::log("--- all tests finished ---");
					_env.parent().exit(_success ? 0 : 1);
					break;
				}
			}
		}
	}

	Genode::Signal_handler<Main> _finished_sigh {
		_env.ep(), *this, &Main::_handle_finished };

	void _construct_tests(Genode::Xml_node config)
	{
		try {
			Genode::Xml_node tests = config.sub_node("tests");
			tests.for_each_sub_node([&] (Genode::Xml_node node) {

				if (node.has_type("ping_pong")) {
					Test_base *t = new (&_heap)
						Ping_pong(_env, _heap, node, _finished_sigh);
					_tests.enqueue(t);
				} else

				if (node.has_type("random")) {
					Test_base *t = new (&_heap)
						Random(_env, _heap, node, _finished_sigh);
					_tests.enqueue(t);
				} else

				if (node.has_type("replay")) {
					Test_base *t = new (&_heap)
						Replay(_env, _heap, node, _finished_sigh);
					_tests.enqueue(t);
				} else

				if (node.has_type("sequential")) {
					Test_base *t = new (&_heap)
						Sequential(_env, _heap, node, _finished_sigh);
					_tests.enqueue(t);
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
		Test_result * tr = nullptr;
		while ((tr = _results.dequeue())) {
			Genode::destroy(&_heap, tr);
		}
	}

	private:

		Main(const Main&) = delete;
		Main& operator=(const Main&) = delete;
};


void Component::construct(Genode::Env &env)
{
	static Test::Main main(env);
}
