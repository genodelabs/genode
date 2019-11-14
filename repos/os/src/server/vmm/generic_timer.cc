/*
 * \brief  VMM ARM Generic timer device model
 * \author Stefan Kalkowski
 * \date   2019-08-20
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <cpu.h>
#include <generic_timer.h>

using Vmm::Generic_timer;

bool Generic_timer::_enabled() {
	return Ctrl::Enabled::get(_cpu.state().timer.control); }


bool Generic_timer::_masked()  {
	return Ctrl::Imask::get(_cpu.state().timer.control);   }


bool Generic_timer::_pending() {
	return Ctrl::Istatus::get(_cpu.state().timer.control); }


void Generic_timer::_handle_timeout(Genode::Duration)
{
	_cpu.handle_signal([this] (void) {
		if (_enabled() && !_masked()) handle_irq();
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
	_cpu.state().timer.irq = true;
	_irq.handler(*this);
}


void Generic_timer::schedule_timeout()
{
	if (_pending()) {
		handle_irq();
		return;
	}

	if (_enabled()) {
		if (_usecs_left()) {
			_timeout.schedule(Genode::Microseconds(_usecs_left()));
		} else _handle_timeout(Genode::Duration(Genode::Microseconds(0)));
	}
}


void Generic_timer::cancel_timeout()
{
	if (_timeout.scheduled()) _timeout.discard();
}


void Generic_timer::handle_irq()
{
	_irq.assert();
	_cpu.state().timer.irq = false;
}


void Generic_timer::eoi()
{
	_cpu.state().timer.irq = true;
};


void Generic_timer::dump()
{
	using namespace Genode;

	log("  timer.ctl  = ", Hex(_cpu.state().timer.control, Hex::PREFIX, Hex::PAD));
	log("  timer.cmp  = ", Hex(_cpu.state().timer.compare, Hex::PREFIX, Hex::PAD));
}
