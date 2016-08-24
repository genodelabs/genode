/*
 * \brief  Cpu_thread_component implementation
 * \author Christian Prochaska
 * \date   2016-01-19
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/snprintf.h>

/* local includes */
#include "cpu_session_component.h"

static constexpr bool verbose_take_sample = false;

using namespace Genode;

Cpu_sampler::Cpu_thread_component::Cpu_thread_component(
                                Cpu_session_component   &cpu_session_component,
                                Allocator               &md_alloc,
                                Pd_session_capability    pd,
                                Cpu_session::Name const &name,
                                Affinity::Location       affinity,
                                Cpu_session::Weight      weight,
                                addr_t                   utcb,
                                char              const *thread_name,
                                unsigned int             thread_id)
: _cpu_session_component(cpu_session_component),
  _md_alloc(md_alloc),
  _parent_cpu_thread(
      _cpu_session_component.parent_cpu_session().create_thread(pd,
                                                                name,
                                                                affinity,
                                                                weight,
                                                                utcb))
{
	char label_buf[Session_label::size()];

	snprintf(label_buf, sizeof(label_buf), "%s -> %s",
	         _cpu_session_component.session_label().string(),
	         thread_name);

	_label = Session_label(label_buf);

	snprintf(label_buf, sizeof(label_buf), "samples -> %s.%u",
             _label.string(), thread_id);

    _log_session_label = Session_label(label_buf);

	_cpu_session_component.thread_ep().manage(this);
}


Cpu_sampler::Cpu_thread_component::~Cpu_thread_component()
{
	flush();

	if (_log)
		destroy(_md_alloc, _log);

	_cpu_session_component.thread_ep().dissolve(this);
}


void Cpu_sampler::Cpu_thread_component::take_sample()
{
	if (verbose_take_sample)
		Genode::log("taking sample of thread ", _label.string());

	if (!_started) {
		if (verbose_take_sample)
			Genode::log("cannot take sample, thread not started yet");
		return;
	}

	try {

		_parent_cpu_thread.pause();

		Thread_state thread_state = _parent_cpu_thread.state();

		_parent_cpu_thread.resume();

		_sample_buf[_sample_buf_index++] = thread_state.ip;

		if (_sample_buf_index == SAMPLE_BUF_SIZE)
			flush();

	} catch (Cpu_thread::State_access_failed) {

		Genode::log("thread state access failed");

	}
}


void Cpu_sampler::Cpu_thread_component::reset()
{
	_sample_buf_index = 0;
}


void Cpu_sampler::Cpu_thread_component::flush()
{
	if (_sample_buf_index == 0)
		return;

	if (!_log)
		_log = new (_md_alloc) Log_connection(_log_session_label);

	/* number of hex characters + newline + '\0' */
	enum { SAMPLE_STRING_SIZE = 2 * sizeof(addr_t) + 1 + 1 };

	char sample_string[SAMPLE_STRING_SIZE];

	char const *format_string;

	if (sizeof(addr_t) == 8)
		format_string = "%16lX\n";
	else
		format_string = "%8X\n";

	for (unsigned int i = 0; i < _sample_buf_index; i++) {
		snprintf(sample_string, SAMPLE_STRING_SIZE, format_string,
		         _sample_buf[i]);
		_log->write(sample_string);
	}

	_sample_buf_index = 0;
}


Dataspace_capability
Cpu_sampler::Cpu_thread_component::utcb()
{
	return _parent_cpu_thread.utcb();
}


void Cpu_sampler::Cpu_thread_component::start(addr_t ip, addr_t sp)
{
	_parent_cpu_thread.start(ip, sp);
	_started = true;
}


void Cpu_sampler::Cpu_thread_component::pause()
{
	_parent_cpu_thread.pause();
}


void Cpu_sampler::Cpu_thread_component::resume()
{
	_parent_cpu_thread.resume();
}


void Cpu_sampler::Cpu_thread_component::single_step(bool enable)
{
	_parent_cpu_thread.single_step(enable);
}


void Cpu_sampler::Cpu_thread_component::cancel_blocking()
{
	_parent_cpu_thread.cancel_blocking();
}


Thread_state Cpu_sampler::Cpu_thread_component::state()
{
	return _parent_cpu_thread.state();
}


void Cpu_sampler::Cpu_thread_component::state(Thread_state const &state)
{
	_parent_cpu_thread.state(state);
}


void
Cpu_sampler::Cpu_thread_component::exception_sigh(Signal_context_capability sigh_cap)
{
	_parent_cpu_thread.exception_sigh(sigh_cap);
}


void Cpu_sampler::Cpu_thread_component::affinity(Affinity::Location location)
{
	_parent_cpu_thread.affinity(location);
}


unsigned Cpu_sampler::Cpu_thread_component::trace_control_index()
{
	return _parent_cpu_thread.trace_control_index();
}


Dataspace_capability
Cpu_sampler::Cpu_thread_component::trace_buffer()
{
	return _parent_cpu_thread.trace_buffer();
}


Dataspace_capability
Cpu_sampler::Cpu_thread_component::trace_policy()
{
	return _parent_cpu_thread.trace_policy();
}
