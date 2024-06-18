/*
 * \brief  Low-level test for TRACE service
 * \author Norman Feske
 * \author Josef Soentgen
 * \author Martin Stein
 * \date   2013-08-12
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <trace_session/connection.h>
#include <timer_session/connection.h>
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/heap.h>
#include <base/sleep.h>
#include <trace/trace_buffer.h>

using namespace Genode;


struct Test_thread : Thread
{
	Env               &_env;
	Timer::Connection  _timer { _env };
	bool               _stop  { false };

	void entry() override
	{
		for (unsigned i = 0; !_stop; i++) {
			if (i & 0x3) {
				Ram_dataspace_capability ds_cap = _env.ram().alloc(1024);
				_env.ram().free(ds_cap);
			}
			_timer.msleep(250);
		}
	}

	Test_thread(Env &env, Name const &name)
	: Thread(env, name, 1024 * sizeof(addr_t)), _env(env) { start(); }

	~Test_thread()
	{
		_stop = true;
		this->join();
	}

};


class Trace_buffer_monitor
{
	private:

		/*
		 * Noncopyable
		 */
		Trace_buffer_monitor(Trace_buffer_monitor const &);
		Trace_buffer_monitor &operator = (Trace_buffer_monitor const &);

		static constexpr size_t MAX_ENTRY_BUF = 256;

		char                  _buf[MAX_ENTRY_BUF];
		Region_map           &_rm;
		Trace::Subject_id     _id;
		Trace::Buffer        *_buffer_raw;
		Trace_buffer          _buffer;

		const char *_terminate_entry(Trace::Buffer::Entry const &entry)
		{
			size_t len = min(entry.length() + 1, MAX_ENTRY_BUF);
			memcpy(_buf, entry.data(), len);
			_buf[len-1] = '\0';
			return _buf;
		}

	public:

		Trace_buffer_monitor(Region_map           &rm,
		                     Trace::Subject_id     id,
		                     Dataspace_capability  ds_cap)
		:
			_rm(rm), _id(id),
			_buffer_raw(rm.attach(ds_cap, {
				.size       = { }, .offset    = { },
				.use_at     = { }, .at        = { },
				.executable = { }, .writeable = true
			}).convert<Trace::Buffer *>(
				[&] (Region_map::Range r)      { return (Trace::Buffer *)r.start; },
				[&] (Region_map::Attach_error) { return nullptr; }
			)),
			_buffer(*_buffer_raw)
		{
			log("monitor "
				"subject:", _id.id, " "
				"buffer:",  Hex((addr_t)_buffer_raw));
		}

		~Trace_buffer_monitor()
		{
			if (_buffer_raw) { _rm.detach(addr_t(_buffer_raw)); }
		}

		Trace::Subject_id id() { return _id; };

		void dump()
		{
			log("read all remaining events");
			_buffer.for_each_new_entry([&] (Trace::Buffer::Entry &entry) {
				char const * const data = _terminate_entry(entry);
				if (data) { log(data); }

				return true;
			});
		}
};


struct Test_out_of_metadata
{
	Env &env;

	struct Test_thread : Genode::Thread
	{
		Test_thread(Genode::Env &env, const char * name)
		: Thread(env, Thread::Name(name), 4096) { start(); }

		~Test_thread() { join(); }

		void entry() override { }
	};

	Test_out_of_metadata(Env &env) : env(env)
	{
		log("test Out_of_ram exception of Trace::Session::subjects call");

		/*
		 * The call of 'subjects' will prompt core's TRACE service to import those
		 * threads as trace subjects into the TRACE session. This step should fail
		 * because we dimensioned the TRACE session with a very low amount of
		 * session quota. The allocation failure is propagated to the TRACE client
		 * by the 'Out_of_ram' exception. The test validates this
		 * error-handling procedure.
		 */

		static constexpr unsigned MAX_SUBJECT_IDS = 16;
		Trace::Subject_id subject_ids[MAX_SUBJECT_IDS];

		try {
			Trace::Connection trace(env, sizeof(subject_ids) + 4096,
			                        sizeof(subject_ids));

			/* we should never arrive here */
			struct Unexpectedly_got_no_exception{};
			throw  Unexpectedly_got_no_exception();
		}
		catch (Service_denied) {
			log("got Service_denied exception as expected"); }

		try {
			/*
			 * Create multiple threads because on some platforms there
			 * are not enough available subjects to trigger the Out_of_ram
			 * exception.
			 */
			Test_thread thread1 { env, "test-thread1" };
			Test_thread thread2 { env, "test-thread2" };
			Test_thread thread3 { env, "test-thread3" };
			Test_thread thread4 { env, "test-thread4" };
			Test_thread thread5 { env, "test-thread5" };

			Trace::Connection trace(env, sizeof(subject_ids) + 5*4096,
			                        sizeof(subject_ids));
			trace.subjects(subject_ids, { MAX_SUBJECT_IDS });

			/* we should never arrive here */
			struct Unexpectedly_got_no_exception{};
			throw  Unexpectedly_got_no_exception();

		} catch (Out_of_ram) {
			log("got Trace::Out_of_ram exception as expected"); }

		log("passed Out_of_ram test");
	}
};


struct Test_tracing
{
	using Policy_id = Trace::Connection::Alloc_policy_result;
	using String = Genode::String<64>;

	Env                   &_env;
	Attached_rom_dataspace _config      { _env, "config" };
	Trace::Connection      _trace       { _env, 1024*1024, 64*1024 };
	Timer::Connection      _timer       { _env };
	Test_thread::Name      _thread_name { "test-thread" };
	Test_thread            _thread      { _env, _thread_name };

	static String _trace_policy_attr(Xml_node const &config, auto const &attr_name)
	{
		String result { };
		config.with_optional_sub_node("trace_policy", [&] (Xml_node const &policy) {
			result = policy.attribute_value(attr_name,  String()); });
		return result;
	}

	String const _policy_label  = _trace_policy_attr(_config.xml(), "label");
	String const _policy_module = _trace_policy_attr(_config.xml(), "module");
	String const _policy_thread = _trace_policy_attr(_config.xml(), "thread");

	Policy_id _init_policy()
	{
		log("test Tracing");

		try {
			log("load module: '", _policy_module, "' for label: '", _policy_label, "'");
			Attached_rom_dataspace const rom { _env, _policy_module.string() };

			Policy_id const policy_id = _trace.alloc_policy(Trace::Policy_size { rom.size() });

			Dataspace_capability ds_cap { };
			policy_id.with_result(
				[&] (Trace::Policy_id id) { ds_cap = _trace.policy(id); },
				[&] (Trace::Connection::Alloc_policy_error) { });

			if (!ds_cap.valid()) {
				error("failed to obtain policy buffer");
				throw Failed();
			}

			Attached_dataspace dst { _env.rm(), ds_cap };
			memcpy(dst.local_addr<char>(), rom.local_addr<char const>(), rom.size());

			return policy_id;

		} catch (...) {
			error("could not load module '", _policy_module, "' for "
			      "label '", _policy_label, "'");
			throw Failed();
		}
	}

	Policy_id const _policy_id = _init_policy();

	Constructible<Trace_buffer_monitor> _test_monitor { };

	struct Failed : Genode::Exception { };

	Test_tracing(Env &env) : _env(env)
	{
		/* wait some time before querying the subjects */
		_timer.msleep(1500);

		auto print_info = [this] (Trace::Subject_id id, Trace::Subject_info info) {

			log("ID:",      id.id,                    " "
			    "label:\"", info.session_label(),   "\" "
			    "name:\"",  info.thread_name(),     "\" "
			    "state:",   Trace::Subject_info::state_name(info.state()), " "
			    "policy:",  info.policy_id().id,      " "
			    "thread context time:", info.execution_time().thread_context, " "
			    "scheduling context time:", info.execution_time().scheduling_context, " ",
			    "priority:", info.execution_time().priority, " ",
			    "quantum:", info.execution_time().quantum);
		};

		_trace.for_each_subject_info(print_info);

		auto check_unattached = [this] (Trace::Subject_id id, Trace::Subject_info info) {

			if (info.state() != Trace::Subject_info::UNATTACHED)
				error("Subject ", id.id, " is not UNATTACHED");
		};

		_trace.for_each_subject_info(check_unattached);

		/* enable tracing for test-thread */
		auto enable_tracing = [this, &env] (Trace::Subject_id id,
		                                    Trace::Subject_info info) {

			if (info.session_label() != _policy_label
			 || info.thread_name()   != _policy_thread) {
				return;
			}

			_policy_id.with_result(
				[&] (Trace::Policy_id policy_id) {
					log("enable tracing for "
					    "thread:'", info.thread_name(), "' with "
					    "policy:", policy_id.id);

					Trace::Connection::Trace_result const trace_result =
						_trace.trace(id, policy_id, Trace::Buffer_size{16384});

					trace_result.with_result(
						[&] (Trace::Trace_ok) {
							Dataspace_capability ds_cap = _trace.buffer(id);
							_test_monitor.construct(env.rm(), id, ds_cap);
						},
						[&] (Trace::Connection::Trace_error e) {
							if (e == Trace::Connection::Trace_error::SOURCE_IS_DEAD)
								error("source is dead");

							throw Failed();
						});
				},
				[&] (Trace::Connection::Alloc_policy_error) {
					error("policy alloc failed");
					throw Failed();
				});
		};

		_trace.for_each_subject_info(enable_tracing);

		/* give the test thread some time to run */
		_timer.msleep(1000);

		_trace.for_each_subject_info(print_info);

		/* read events from trace buffer */
		if (_test_monitor.constructed()) {
			_test_monitor->dump();
			_test_monitor.destruct();
			log("passed Tracing test");
		}
		else
			error("Thread '", _policy_thread, "' not found for session ", _policy_label);
	}
};


struct Main
{
	Constructible<Test_out_of_metadata> test_1 { };
	Constructible<Test_tracing>         test_2 { };

	Main(Env &env)
	{
//		test_1.construct(env);
//		test_1.destruct();
		test_2.construct(env);
		test_2.destruct();

		env.parent().exit(0);
	}
};


void Component::construct(Env &env) { static Main main(env); }
