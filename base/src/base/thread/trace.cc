/*
 * \brief  Event-tracing support
 * \author Norman Feske
 * \date   2013-08-09
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/env.h>
#include <base/thread.h>
#include <base/trace/policy.h>
#include <dataspace/client.h>
#include <util/construct_at.h>

/* local includes */
#include <trace/control.h>

using namespace Genode;


namespace Genode { bool inhibit_tracing = true; /* cleared by '_main' */ }


/**
 * Return trace-control structure for specified thread
 */
static Trace::Control *trace_control(Cpu_session *cpu, Rm_session *rm,
                                     Thread_capability thread_cap)
{
	struct Area
	{
		Cpu_session           &cpu;
		Rm_session            &rm;
		Dataspace_capability   ds;
		size_t           const size;
		Trace::Control * const base;

		Area(Cpu_session &cpu, Rm_session &rm)
		:
			cpu(cpu), rm(rm),
			ds(cpu.trace_control()),
			size(ds.valid() ? Dataspace_client(ds).size() : 0),
			base(ds.valid() ? (Trace::Control * const)rm.attach(ds) : 0)
		{ }

		Trace::Control *slot(Thread_capability thread)
		{
			if (!thread.valid() || base == 0)
				return 0;

			unsigned const index = cpu.trace_control_index(thread);

			if ((index + 1)*sizeof(Trace::Control) > size) {
				PERR("thread control index is out of range");
				return 0;
			}

			return base + index;
		}
	};

	/**
	 * We have to construct the Area object explicitly because otherwise
	 * the destructor may use a invalid capability. This is mainly the
	 * case by e.g. forked processes in noux.
	 */

	static char area_mem[sizeof (Area)];
	static Area *area = 0;

	if (!area) {
		area = construct_at<Area>(area_mem, *cpu, *rm);
	}

	return area->slot(thread_cap);
}


/*******************
 ** Trace::Logger **
 *******************/

bool Trace::Logger::_evaluate_control()
{
	/* check process-global and thread-specific tracing condition */
	if (inhibit_tracing || !control || control->tracing_inhibited())
		return false;

	if (control->state_changed()) {

		/* suppress tracing during initialization */
		Control::Inhibit_guard guard(*control);

		if (control->to_be_disabled()) {

			/* unload policy */
			if (policy_module) {
				env()->rm_session()->detach(policy_module);
				policy_module = 0;
			}

			/* unmap trace buffer */
			if (buffer) {
				env()->rm_session()->detach(buffer);
				buffer = 0;
			}

			/* inhibit generation of trace events */
			enabled = false;
			control->acknowledge_disabled();
		}

		else if (control->to_be_enabled()) {

			control->acknowledge_enabled();
			enabled = true;
		}
	}

	if (enabled && (policy_version != control->policy_version())) {

		/* suppress tracing during policy change */
		Control::Inhibit_guard guard(*control);

		/* obtain policy */
		Dataspace_capability policy_ds = env()->cpu_session()->trace_policy(thread_cap);

		if (!policy_ds.valid()) {
			PWRN("could not obtain trace policy");
			control->error();
			enabled = false;
			return false;
		}

		try {
			max_event_size = 0;
			policy_module  = 0;

			policy_module = env()->rm_session()->attach(policy_ds);

			/* relocate function pointers of policy callback table */
			for (unsigned i = 0; i < sizeof(Trace::Policy_module)/sizeof(void *); i++) {
				((addr_t *)policy_module)[i] += (addr_t)(policy_module);
			}

			max_event_size = policy_module->max_event_size();

		} catch (...) { }

		/* obtain buffer */
		buffer = 0;
		Dataspace_capability buffer_ds = env()->cpu_session()->trace_buffer(thread_cap);

		if (!buffer_ds.valid()) {
			PWRN("could not obtain trace buffer");
			control->error();
			enabled = false;
			return false;
		}

		try {
			buffer = env()->rm_session()->attach(buffer_ds);
			buffer->init(Dataspace_client(buffer_ds).size());
		} catch (...) { }

		policy_version = control->policy_version();
	}

	return enabled && policy_module;
}


void Trace::Logger::log(char const *msg, size_t len)
{
	if (!this || !_evaluate_control()) return;

	memcpy(buffer->reserve(len), msg, len);
	buffer->commit(len);
}


void Trace::Logger::init(Thread_capability thread)
{
	thread_cap = thread;

	control = trace_control(env()->cpu_session(), env()->rm_session(), thread);
}


Trace::Logger::Logger()
:
	control(0),
	enabled(false),
	policy_version(0),
	policy_module(0),
	max_event_size(0),
	pending_init(false)
{ }


/*****************
 ** Thread_base **
 *****************/

/**
 * return logger instance for the main thread **
 */
static Trace::Logger *main_trace_logger()
{
	static Trace::Logger logger;
	return &logger;
}


Trace::Logger *Thread_base::_logger()
{
	if (inhibit_tracing)
		return 0;

	Thread_base * const myself = Thread_base::myself();

	Trace::Logger * const logger = myself ? &myself->_trace_logger
	                                      : main_trace_logger();

	/* logger is already being initialized */
	if (logger->is_init_pending())
		return logger;

	/* lazily initialize trace object */
	if (!logger->is_initialized()) {
		logger->init_pending(true);
		logger->init(myself ? myself->_thread_cap : env()->parent()->main_thread_cap());
	}

	return logger;
}

