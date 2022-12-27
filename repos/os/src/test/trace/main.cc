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
	Env               &env;
	Timer::Connection  timer { env };
	bool               stop  { false };

	void entry() override
	{
		for (unsigned i = 0; !stop; i++) {
			if (i & 0x3) {
				Ram_dataspace_capability ds_cap = env.ram().alloc(1024);
				env.ram().free(ds_cap);
			}
			timer.msleep(250);
		}
	}

	Test_thread(Env &env, Name &name)
	: Thread(env, name, 1024 * sizeof(addr_t)), env(env) { start(); }

	~Test_thread()
	{
		stop = true;
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
			_rm(rm), _id(id), _buffer_raw(rm.attach(ds_cap)),
			_buffer(*_buffer_raw)
		{
			log("monitor "
				"subject:", _id.id, " "
				"buffer:",  Hex((addr_t)_buffer_raw));
		}

		~Trace_buffer_monitor()
		{
			if (_buffer_raw) { _rm.detach(_buffer_raw); }
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

		enum { MAX_SUBJECT_IDS = 16 };
		Trace::Subject_id subject_ids[MAX_SUBJECT_IDS];

		try {
			Trace::Connection trace(env, sizeof(subject_ids) + 4096,
			                        sizeof(subject_ids), 0);

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
			                        sizeof(subject_ids), 0);
			trace.subjects(subject_ids, MAX_SUBJECT_IDS);

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
	Env                     &env;
	Attached_rom_dataspace   config       { env, "config" };
	Trace::Connection        trace        { env, 1024*1024, 64*1024, 0 };
	Timer::Connection        timer        { env };
	Test_thread::Name        thread_name  { "test-thread" };
	Test_thread              thread       { env, thread_name };
	Trace::Policy_id         policy_id    { };

	Constructible<Trace_buffer_monitor> test_monitor { };

	typedef Genode::String<64> String;
	String policy_label  { };
	String policy_module { };
	String policy_thread { };

	Rom_dataspace_capability  policy_module_rom_ds { };

	struct Failed : Genode::Exception { };

	Test_tracing(Env &env) : env(env)
	{
		enum { MAX_SUBJECTS = 128 };

		log("test Tracing");

		try {
			Xml_node policy = config.xml().sub_node("trace_policy");
			policy.attribute("label").value(policy_label);
			policy.attribute("module").value(policy_module);
			policy.attribute("thread").value(policy_thread);

			Rom_connection policy_rom(env, policy_module.string());
			policy_module_rom_ds = policy_rom.dataspace();

			size_t rom_size = Dataspace_client(policy_module_rom_ds).size();

			policy_id = trace.alloc_policy(rom_size);
			Dataspace_capability ds_cap = trace.policy(policy_id);

			if (ds_cap.valid()) {
				void *ram = env.rm().attach(ds_cap);
				void *rom = env.rm().attach(policy_module_rom_ds);
				memcpy(ram, rom, rom_size);

				env.rm().detach(ram);
				env.rm().detach(rom);
			}

			log("load module: '", policy_module, "' for "
			    "label: '", policy_label, "'");
		} catch (...) {
			error("could not load module '", policy_module, "' for "
			      "label '", policy_label, "'");
			throw Failed();
		}

		/* wait some time before querying the subjects */
		timer.msleep(1500);

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

		trace.for_each_subject_info(print_info);

		auto check_unattached = [this] (Trace::Subject_id id, Trace::Subject_info info) {

			if (info.state() != Trace::Subject_info::UNATTACHED)
				error("Subject ", id.id, " is not UNATTACHED");
		};

		trace.for_each_subject_info(check_unattached);

		/* enable tracing for test-thread */
		auto enable_tracing = [this, &env] (Trace::Subject_id id,
		                                    Trace::Subject_info info) {

			if (info.session_label() != policy_label
			 || info.thread_name()   != policy_thread) {
				return;
			}

			try {
				log("enable tracing for "
				    "thread:'", info.thread_name().string(), "' with "
				    "policy:", policy_id.id);

				trace.trace(id.id, policy_id, 16384U);

				Dataspace_capability ds_cap = trace.buffer(id.id);
				test_monitor.construct(env.rm(), id.id, ds_cap);

			} catch (Trace::Source_is_dead) {
				error("source is dead");
				throw Failed();
			}
		};

		trace.for_each_subject_info(enable_tracing);

		/* give the test thread some time to run */
		timer.msleep(1000);

		trace.for_each_subject_info(print_info);

		/* read events from trace buffer */
		if (test_monitor.constructed()) {
			test_monitor->dump();
			test_monitor.destruct();
			log("passed Tracing test");
		}
		else
			error("Thread '", policy_thread, "' not found for session ", policy_label);
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
