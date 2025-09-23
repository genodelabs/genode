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
#include <trace_session/connection.h>
#include <timer_session/connection.h>

using namespace Genode;

struct Main
{
	Env                    &env;
	Attached_rom_dataspace  config { env, "config" };
	Timer::Connection       timer { env };

	unsigned const prio_levels_log2   { _prio_levels_from_node(config.node(), env) };
	long const     prio_max           { (1 << prio_levels_log2) - 1 };

	long const     platform_prio_size { Cpu_session::PRIORITY_LIMIT >> prio_levels_log2 };
	long const     highest_prio_end   { platform_prio_size-1 };
	long const     lowest_prio_start  { prio_max * platform_prio_size };

	size_t arg_buffer_ram  { 4*4096 };
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

	/* wait for dummy components */
	timer.msleep(1000);

	/* query thread priorities from TRACE */
	unsigned low_prio   = ~0U;
	unsigned low2_prio  = ~0U;
	unsigned high_prio  = ~0U;
	unsigned high2_prio = ~0U;
	trace.for_each_subject_info([&](Trace::Subject_id const &,
	                                Trace::Subject_info const &info)
	{
		if (info.thread_name() == "lowest")
			low_prio = info.execution_time().priority;
		else if (info.thread_name() == "second lowest")
			low2_prio = info.execution_time().priority;
		else if (info.thread_name() == "highest")
			high_prio = info.execution_time().priority;
		else if (info.thread_name() == "second highest")
			high2_prio = info.execution_time().priority;
	});

	auto check_priority = [&](long const expected, long const current,
	                          String<32> const &name) {
		if (expected != current) {
			error("Unexpected priority of component ", name,
			      " expected ", expected, " got ", current);
			return false;
		}

		return true;
	};

	bool success = true;
	if (!inverse) {
		success &= check_priority(prio_max + offset,     low_prio,   "lowest");
		success &= check_priority(prio_max + offset - 1, low2_prio,  "second lowest");
		success &= check_priority(offset,                high_prio,  "highest");
		success &= check_priority(offset + 1,            high2_prio, "second highest");
	} else {
		success &= check_priority(offset,                low_prio,   "lowest");
		success &= check_priority(offset + 1,            low2_prio,  "second lowest");
		success &= check_priority(prio_max + offset,     high_prio,  "highest");
		success &= check_priority(prio_max + offset - 1, high2_prio, "second highest");
	}

	if (success)
		env.parent().exit(0);
	else
		env.parent().exit(1);
}

void Component::construct(Genode::Env &env) { static Main main(env); }
