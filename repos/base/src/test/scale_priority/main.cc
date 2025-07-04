/*
 * \brief  Testing CPU priorities
 * \author Johannes Schlatow
 * \date   2025-06-11
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/log.h>
#include <base/thread.h>
#include <cpu_session/connection.h>
#include <trace_session/connection.h>


using namespace Genode;

/******************************************
 ** Using cpu-session for thread creation *
 ******************************************/

struct Cpu_helper : Thread
{
	Env &_env;

	Cpu_helper(Env &env, Name const &name, Cpu_session &cpu)
	:
		Thread(env, name, 4096, Thread::Location(), Thread::Weight(), cpu),
		_env(env)
	{ }

	void entry() override
	{
		log(Thread::name);
	}
};


struct Main
{
	Env                    &env;
	Attached_rom_dataspace  config { env, "config" };

	unsigned const prio_levels_log2   { _prio_levels_from_node(config.node(), env) };
	long const     prio_max           { (1 << prio_levels_log2) - 1 };

	long const     platform_prio_size { Cpu_session::PRIORITY_LIMIT >> prio_levels_log2 };
	long const     highest_prio_end   { platform_prio_size-1 };
	long const     lowest_prio_start  { prio_max * platform_prio_size };


	Cpu_connection cpu_high  { env, "highest",        highest_prio_end };
	Cpu_connection cpu_high2 { env, "second highest", highest_prio_end+1 };
	Cpu_connection cpu_low   { env, "lowest",         lowest_prio_start };
	Cpu_connection cpu_low2  { env, "second lowest",  lowest_prio_start-1 };

	Cpu_helper     thread_high  { env, "highest",        cpu_high };
	Cpu_helper     thread_high2 { env, "second highest", cpu_high2 };
	Cpu_helper     thread_low   { env, "lowest",         cpu_low };
	Cpu_helper     thread_low2  { env, "second lowest",  cpu_low2 };

	size_t arg_buffer_ram  { 4096 };
	size_t trace_ram_quota { arg_buffer_ram + 4 * 4096 };

	Trace::Connection trace { env,
	                          trace_ram_quota,
	                          arg_buffer_ram };

	static unsigned _prio_levels_from_node(Node const &node, Env &env)
	{
		unsigned levels = node.attribute_value("prio_levels_log2", 0U);
		if (levels == 0) {
			error("Missing or invalid config attribute 'prio_levels_log2'.");
			env.parent().exit(1);
		}

		return levels;
	}

	Main(Env &);
};

Main::Main(Env &env) : env(env)
{
	bool const start_at_zero = config.node().attribute_value("start_at_zero", true);
	bool const inverse       = config.node().attribute_value("inverse", true);
	unsigned const offset    = start_at_zero ? 0 : 1;

	thread_low.start();
	thread_low2.start();
	thread_high.start();
	thread_high2.start();

	/* query thread priorities from TRACE */
	unsigned low_prio   = ~0U;
	unsigned low2_prio  = ~0U;
	unsigned high_prio  = ~0U;
	unsigned high2_prio = ~0U;
	trace.for_each_subject_info([&](Trace::Subject_id const &,
	                                Trace::Subject_info const &info)
	{
		if (info.thread_name() == thread_low.name)
			low_prio = info.execution_time().priority;
		else if (info.thread_name() == thread_low2.name)
			low2_prio = info.execution_time().priority;
		else if (info.thread_name() == thread_high.name)
			high_prio = info.execution_time().priority;
		else if (info.thread_name() == thread_high2.name)
			high2_prio = info.execution_time().priority;
	});

	auto check_priority = [&](long const expected, long const current,
	                          Thread const &thread) {
		if (expected != current) {
			error("Unexpected priority of Thread ", thread.name,
			      " expected ", expected, " got ", current);
			return false;
		}

		return true;
	};

	bool success = true;
	if (!inverse) {
		success &= check_priority(prio_max + offset,     low_prio,   thread_low);
		success &= check_priority(prio_max + offset - 1, low2_prio,  thread_low2);
		success &= check_priority(offset,                high_prio,  thread_high);
		success &= check_priority(offset + 1,            high2_prio, thread_high2);
	} else {
		success &= check_priority(offset,                low_prio,   thread_low);
		success &= check_priority(offset + 1,            low2_prio,  thread_low2);
		success &= check_priority(prio_max + offset,     high_prio,  thread_high);
		success &= check_priority(prio_max + offset - 1, high2_prio, thread_high2);
	}

	thread_low.join();
	thread_low2.join();
	thread_high.join();
	thread_high2.join();

	if (success)
		env.parent().exit(0);
	else
		env.parent().exit(1);
}

void Component::construct(Genode::Env &env) { static Main main(env); }
