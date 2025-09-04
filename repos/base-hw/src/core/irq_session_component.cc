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
#include <platform.h>

/* base-internal includes */
#include <base/internal/capability_space.h>

using namespace Core;


void Irq_session_component::ack_irq()
{
	if (_kobj.constructed()) Kernel::irq_ack(*_kobj);
}


void Irq_session_component::sigh(Signal_context_capability cap)
{
	_irq_number.with_result([&] (Range_allocator::Allocation const &a) {
		unsigned const n = unsigned(addr_t(a.ptr));

		if (_sig_cap.valid()) {
			warning("signal handler already registered for IRQ ", n);
			return;
		}

		_sig_cap = cap;

		if (!_kobj.create(n, _args.trigger(), _args.polarity(),
		                  Capability_space::capid(_sig_cap)))
			warning("invalid signal handler for IRQ ", n);

	}, [&] (Alloc_error) { });
}


void Irq_session_component::revoke_signal_context(Signal_context_capability cap)
{
	if (cap != _sig_cap)
		return;
	_sig_cap = Signal_context_capability();
	if (_kobj.constructed()) _kobj.destruct();
}


Irq_session_component::Msi::Msi(Irq_args const &args)
:
	allocated(args.msi() ? Platform::alloc_msi_vector(address, value) : false)
{ }


Irq_session_component::Msi::~Msi()
{
	if (allocated) Platform::free_msi_vector(address, value);
}


Range_allocator::Result Irq_session_component::_allocate(Range_allocator &irq_alloc) const
{
	if (_args.msi()) {
		if (!_msi.allocated) {
			error("allocation of MSI vector failed");
			return Alloc_error::DENIED;
		}
		return irq_alloc.alloc_addr(1, _msi.value);
	}
	return irq_alloc.alloc_addr(1, Platform::irq(_args.irq_number()));
}
