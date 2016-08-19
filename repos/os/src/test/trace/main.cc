/*
 * \brief  Low-level test for TRACE service
 * \author Norman Feske
 * \author Josef Soentgen
 * \date   2013-08-12
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <trace_session/connection.h>
#include <timer_session/connection.h>
#include <os/config.h>
#include <base/sleep.h>

static char const *state_name(Genode::Trace::Subject_info::State state)
{
	switch (state) {
	case Genode::Trace::Subject_info::INVALID:  return "INVALID";
	case Genode::Trace::Subject_info::UNTRACED: return "UNTRACED";
	case Genode::Trace::Subject_info::TRACED:   return "TRACED";
	case Genode::Trace::Subject_info::FOREIGN:  return "FOREIGN";
	case Genode::Trace::Subject_info::ERROR:    return "ERROR";
	case Genode::Trace::Subject_info::DEAD:     return "DEAD";
	}
	return "undefined";
}


struct Test_thread : Genode::Thread_deprecated<1024 * sizeof (unsigned long)>
{
	Timer::Connection _timer;

	void entry()
	{
		using namespace Genode;

		for (size_t i = 0; ; i++) {
			if (i & 0x3) {
				Ram_dataspace_capability ds_cap = env()->ram_session()->alloc(1024);
				env()->ram_session()->free(ds_cap);
			}

			_timer.msleep(250);
		}
	}

	Test_thread(const char *name)
	: Thread_deprecated(name) { start(); }
};


using namespace Genode;


class Trace_buffer_monitor
{
	private:

		enum { MAX_ENTRY_BUF = 256 };
		char                  _buf[MAX_ENTRY_BUF];

		Trace::Subject_id     _id;
		Trace::Buffer        *_buffer;
		Trace::Buffer::Entry _curr_entry;

		const char *_terminate_entry(Trace::Buffer::Entry const &entry)
		{
			size_t len = min(entry.length() + 1, MAX_ENTRY_BUF);
			memcpy(_buf, entry.data(), len);
			_buf[len-1] = '\0';

			return _buf;
		}

	public:

		Trace_buffer_monitor(Trace::Subject_id id, Dataspace_capability ds_cap)
		:
			_id(id),
			_buffer(env()->rm_session()->attach(ds_cap)),
			_curr_entry(_buffer->first())
		{
			log("monitor subject:", _id.id, " buffer:", Hex((addr_t)_buffer));
		}

		~Trace_buffer_monitor()
		{
			if (_buffer)
				env()->rm_session()->detach(_buffer);
		}

		Trace::Subject_id id() { return _id; };

		void dump()
		{
			log("overflows: ", _buffer->wrapped());

			log("read all remaining events");
			for (; !_curr_entry.last(); _curr_entry = _buffer->next(_curr_entry)) {
				/* omit empty entries */
				if (_curr_entry.length() == 0)
					continue;

				const char *data = _terminate_entry(_curr_entry);
				if (data)
					log(data);
			}

			/* reset after we read all available entries */
			_curr_entry = _buffer->first();
		}
};


static void test_out_of_metadata()
{
	log("test Out_of_metadata exception of Trace::Session::subjects call");

	/*
	 * The call of 'subjects' will prompt core's TRACE service to import those
	 * threads as trace subjects into the TRACE session. This step should fail
	 * because we dimensioned the TRACE session with a very low amount of
	 * session quota. The allocation failure is propagated to the TRACE client
	 * by the 'Out_of_metadata' exception. The test validates this
	 * error-handling procedure.
	 */

	enum { MAX_SUBJECT_IDS = 16 };
	Genode::Trace::Subject_id subject_ids[MAX_SUBJECT_IDS];

	try {
		Genode::Trace::Connection trace(sizeof(subject_ids) + 4096, sizeof(subject_ids), 0);

		/* we should never arrive here */
		struct Unexpectedly_got_no_exception{};
		throw  Unexpectedly_got_no_exception();
	} catch (Genode::Parent::Service_denied) {
		log("got Genode::Parent::Service_denied exception as expected");
	}

	try {
		Genode::Trace::Connection trace(sizeof(subject_ids) + 5*4096, sizeof(subject_ids), 0);
		trace.subjects(subject_ids, MAX_SUBJECT_IDS);

		/* we should never arrive here */
		struct Unexpectedly_got_no_exception{};
		throw  Unexpectedly_got_no_exception();

	} catch (Trace::Out_of_metadata) {
		log("got Trace::Out_of_metadata exception as expected");
	}

	log("passed Out_of_metadata test");
}


int main(int argc, char **argv)
{
	using namespace Genode;

	log("--- test-trace started ---");

	test_out_of_metadata();

	static Genode::Trace::Connection trace(1024*1024, 64*1024, 0);

	static Timer::Connection timer;

	static Test_thread test("test-thread");

	static Trace_buffer_monitor *test_monitor = 0;

	Genode::Trace::Policy_id policy_id;
	bool                     policy_set = false;

	char                     policy_label[64];
	char                     policy_module[64];
	Rom_dataspace_capability policy_module_rom_ds;

	try {
		Xml_node policy = config()->xml_node().sub_node("trace_policy");
		for (;; policy = policy.next("trace_policy")) {
			try {
				policy.attribute("label").value(policy_label, sizeof (policy_label));
				policy.attribute("module").value(policy_module, sizeof (policy_module));

				static Rom_connection policy_rom(policy_module);
				policy_module_rom_ds = policy_rom.dataspace();

				size_t rom_size = Dataspace_client(policy_module_rom_ds).size();

				policy_id = trace.alloc_policy(rom_size);
				Dataspace_capability ds_cap = trace.policy(policy_id);

				if (ds_cap.valid()) {
					void *ram = env()->rm_session()->attach(ds_cap);
					void *rom = env()->rm_session()->attach(policy_module_rom_ds);
					memcpy(ram, rom, rom_size);

					env()->rm_session()->detach(ram);
					env()->rm_session()->detach(rom);
				}
			} catch (...) {
				error("could not load module '", Cstring(policy_module), "' for "
				      "label '", Cstring(policy_label), "'");
			}

			log("load module: '", Cstring(policy_module), "' for "
			    "label: '", Cstring(policy_label), "'");

			if (policy.last("trace_policy")) break;
		}

	} catch (...) { }

	for (size_t cnt = 0; cnt < 5; cnt++) {

		timer.msleep(3000);

		Trace::Subject_id subjects[32];
		size_t num_subjects = trace.subjects(subjects, 32);

		log(num_subjects, " tracing subjects present");

		for (size_t i = 0; i < num_subjects; i++) {

			Trace::Subject_info info = trace.subject_info(subjects[i]);
			log("ID:",      subjects[i].id,           " "
			    "label:\"", info.session_label(),   "\" "
			    "name:\"",  info.thread_name(),     "\" "
			    "state:",   state_name(info.state()), " "
			    "policy:",  info.policy_id().id,      " "
			    "time:",    info.execution_time().value);

			/* enable tracing */
			if (!policy_set
			    && strcmp(info.session_label().string(), policy_label) == 0
			    && strcmp(info.thread_name().string(), "test-thread") == 0) {
				try {
					log("enable tracing for "
					    "thread:'", info.thread_name().string(), "' with "
					    "policy:", policy_id.id);

					trace.trace(subjects[i].id, policy_id, 16384U);

					Dataspace_capability ds_cap = trace.buffer(subjects[i].id);
					test_monitor = new (env()->heap()) Trace_buffer_monitor(subjects[i].id, ds_cap);

				} catch (Trace::Source_is_dead) { error("source is dead"); }

				policy_set = true;
			}

			/* read events from trace buffer */
			if (test_monitor) {
				if (subjects[i].id == test_monitor->id().id)
					test_monitor->dump();
			}
		}
	}

	if (test_monitor)
		destroy(env()->heap(), test_monitor);

	log("--- test-trace finished ---");
	return 0;
}
