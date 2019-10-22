/*
 * \brief  Backend for IRQ sessions served by core
 * \author Martin Stein
 * \author Reto Buerki
 * \date   2012-02-12
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* base-hw includes */
#include <kernel/core_interface.h>

/* core includes */
#include <kernel/irq.h>
#include <irq_root.h>
#include <core_env.h>

/* base-internal includes */
#include <base/internal/capability_space.h>

using namespace Genode;


unsigned Irq_session_component::_find_irq_number(const char * const args)
{
	return Arg_string::find_arg(args, "irq_number").long_value(-1);
}


void Irq_session_component::ack_irq()
{
	if (_kobj.constructed()) Kernel::ack_irq(*_kobj);
}


void Irq_session_component::sigh(Signal_context_capability cap)
{
	if (_sig_cap.valid()) {
		warning("signal handler already registered for IRQ ", _irq_number);
		return;
	}

	_sig_cap = cap;

	if (!_kobj.create(_irq_number, _irq_args.trigger(), _irq_args.polarity(),
	                  Capability_space::capid(_sig_cap)))
		warning("invalid signal handler for IRQ ", _irq_number);
}


Irq_session_component::~Irq_session_component()
{
	using namespace Kernel;

	_irq_alloc.free((void *)(addr_t)_irq_number);
}


Irq_session_component::Irq_session_component(Range_allocator &irq_alloc,
                                             const char * const args)
: _irq_args(args),
  _irq_number(Platform::irq(_irq_args.irq_number())),
  _irq_alloc(irq_alloc),
  _kobj(), _is_msi(false), _address(0), _value(0)
{
	const long mmconf =
		Arg_string::find_arg(args, "device_config_phys").long_value(0);

	if (mmconf) {
		_is_msi =
			Platform::get_msi_params(mmconf, _address, _value, _irq_number);
		if (!_is_msi)
			throw Service_denied();
	}

	/* allocate interrupt */
	if (_irq_alloc.alloc_addr(1, _irq_number).error()) {
		error("unavailable interrupt ", _irq_number, " requested");
		throw Service_denied();
	}
}
