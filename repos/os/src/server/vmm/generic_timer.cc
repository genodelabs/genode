/*
 * \brief  VMM ARM Generic timer device model
 * \author Stefan Kalkowski
 * \author Benjamin Lamowski
 * \date   2019-08-20
 */

/*
 * Copyright (C) 2019-2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <cpu.h>
#include <generic_timer.h>

using Vmm::Generic_timer;

bool Generic_timer::_enabled(Vm_state &state)
{
	return Ctrl::Enabled::get(state.timer.control);
}

bool Generic_timer::_masked(Vm_state &state)
{
	return Ctrl::Imask::get(state.timer.control);
}


bool Generic_timer::_pending(Vm_state &state)
{
	return Ctrl::Istatus::get(state.timer.control);
}


void Generic_timer::_handle_timeout(Genode::Duration)
{
	_cpu.handle_signal([this](Vm_state &state) {
		if (_enabled(state) && !_masked(state))
			handle_irq(state);
	});
}


Generic_timer::Generic_timer(Genode::Env        & env,
                             Genode::Entrypoint & ep,
                             Gic::Irq           & irq,
                             Cpu_base           & cpu)
: _timer(env, ep),
  _timeout(_timer, *this, &Generic_timer::_handle_timeout),
  _irq(irq),
  _cpu(cpu)
{
	_irq.handler(*this);
}


void Generic_timer::schedule_timeout(Vm_state &state)
{
	if (_pending(state)) {
		handle_irq(state);
		return;
	}

	if (_enabled(state)) {
		Genode::uint64_t usecs = _usecs_left(state);
		if (usecs) {
			_timeout.schedule(Genode::Microseconds(usecs));
		} else _handle_timeout(Genode::Duration(Genode::Microseconds(0)));
	}
}


void Generic_timer::cancel_timeout()
{
	if (_timeout.scheduled()) { _timeout.discard(); }
}


void Generic_timer::handle_irq(Vm_state &state)
{
	_irq.assert();
	state.timer.irq = false;
}


void Generic_timer::eoi()
{
	Genode::Vm_state &state = _cpu.state();
	state.timer.irq         = false;
};


void Generic_timer::dump(Vm_state &state)
{
	using namespace Genode;

	log("  timer.ctl  = ", Hex(state.timer.control, Hex::PREFIX, Hex::PAD));
	log("  timer.cmp  = ", Hex(state.timer.compare, Hex::PREFIX, Hex::PAD));
}

void Generic_timer::setup_state(Vm_state &state)
{
	state.timer.irq = true;
}
