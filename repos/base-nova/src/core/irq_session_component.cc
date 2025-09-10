/*
 * \brief  Implementation of IRQ session component
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \author Alexander Boettcher
 * \date   2009-10-02
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <irq_root.h>
#include <irq_args.h>
#include <platform.h>

/* NOVA includes */
#include <nova/syscalls.h>
#include <nova_util.h>

using namespace Core;


static bool irq_ctrl(addr_t irq_sel, addr_t &msi_addr, addr_t &msi_data,
                     addr_t sig_sel, Nova::Gsi_flags flags, addr_t virt_addr)
{
	/* assign IRQ to CPU && request msi data to be used by driver */
	uint8_t res = Nova::assign_gsi(irq_sel, virt_addr, boot_cpu(),
	                               msi_addr, msi_data, sig_sel, flags);

	if (res != Nova::NOVA_OK)
		error("setting up MSI failed - error ", res);

	/* nova syscall interface specifies msi addr/data to be 32bit */
	msi_addr = msi_addr & ~0U;
	msi_data = msi_data & ~0U;

	return res == Nova::NOVA_OK;
}


static bool associate_gsi(addr_t irq_sel, Signal_context_capability sig_cap,
                          Nova::Gsi_flags gsi_flags)
{
	addr_t dummy1 = 0, dummy2 = 0;

	return irq_ctrl(irq_sel, dummy1, dummy2, sig_cap.local_name(), gsi_flags, 0);
}


static void deassociate(addr_t irq_sel)
{
	addr_t dummy1 = 0, dummy2 = 0;

	if (!irq_ctrl(irq_sel, dummy1, dummy2, irq_sel, Nova::Gsi_flags(), 0))
		warning("Irq could not be de-associated");
}


static bool associate_msi(addr_t irq_sel, addr_t phys_mem, addr_t &msi_addr,
                          addr_t &msi_data, Signal_context_capability sig_cap)
{
	using Virt_allocation = Range_allocator::Allocation;

	if (!phys_mem)
		return irq_ctrl(irq_sel, msi_addr, msi_data, sig_cap.local_name(), Nova::Gsi_flags(), 0);

	return platform().region_alloc().alloc_aligned(4096, 12).convert<bool>(

		[&] (Virt_allocation &virt) {

			addr_t const virt_addr = reinterpret_cast<addr_t>(virt.ptr);

			using Nova::Rights;
			using Nova::Utcb;

			Nova::Mem_crd phys_crd(phys_mem  >> 12, 0, Rights(true, false, false));
			Nova::Mem_crd virt_crd(virt_addr >> 12, 0, Rights(true, false, false));

			Utcb &utcb = *reinterpret_cast<Utcb *>(Thread::myself()->utcb());

			if (map_local_phys_to_virt(utcb, phys_crd, virt_crd, platform_specific().core_pd_sel()))
				return false;

			/* try to assign MSI to device */
			bool res = irq_ctrl(irq_sel, msi_addr, msi_data, sig_cap.local_name(), Nova::Gsi_flags(), virt_addr);

			unmap_local(Nova::Mem_crd(virt_addr >> 12, 0, Rights(true, true, true)));

			return res;
		},
		[&] (Alloc_error) {
			return false;
		});
}


void Irq_object::sigh(Signal_context_capability cap)
{
	if (!_sigh_cap.valid() && !cap.valid())
		return;

	if (_sigh_cap.valid() && _sigh_cap == cap) {
		/* avoid useless overhead, e.g. with IOMMUs enabled */
		return;
	}

	if ((_sigh_cap.valid() && !cap.valid())) {
		deassociate(irq_sel());
		_sigh_cap = Signal_context_capability();
		return;
	}

	/* associate GSI or MSI to device belonging to device_phys */
	bool ok = false;
	if (_device_phys || (_msi_addr && _msi_data))
		ok = associate_msi(irq_sel(), _device_phys, _msi_addr, _msi_data, cap);
	else
		ok = associate_gsi(irq_sel(), cap, _gsi_flags);

	if (!ok) {
		deassociate(irq_sel());
		_sigh_cap = Signal_context_capability();
		return;
	}

	_sigh_cap = cap;
}


void Irq_object::ack_irq()
{
	if (Nova::NOVA_OK != Nova::sm_ctrl(irq_sel(), Nova::SEMAPHORE_DOWN))
		error("Unmasking irq of selector ", irq_sel(), " failed");
}


void Irq_object::start(unsigned irq, addr_t const device_phys, Irq_args const &irq_args)
{
	/* map IRQ SM cap from kernel to core at irq_sel selector */
	using Nova::Obj_crd;

	Obj_crd src(platform_specific().gsi_base_sel() + irq, 0);
	Obj_crd dst(irq_sel(), 0);
	enum { MAP_FROM_KERNEL_TO_CORE = true };

	int ret = map_local(platform_specific().core_pd_sel(),
	                    *(Nova::Utcb *)Thread::myself()->utcb(),
	                    src, dst, MAP_FROM_KERNEL_TO_CORE);
	if (ret) {
		error("getting IRQ from kernel failed - ", irq);
		return;
	}

	/* initialize GSI IRQ flags */
	auto gsi_flags = [] (Irq_args const &args)
	{
		if (args.trigger() == Irq_session::TRIGGER_UNCHANGED
		 || args.polarity() == Irq_session::POLARITY_UNCHANGED)
			return Nova::Gsi_flags();

		if (args.trigger() == Irq_session::TRIGGER_EDGE)
			return Nova::Gsi_flags(Nova::Gsi_flags::EDGE);

		if (args.polarity() == Irq_session::POLARITY_HIGH)
			return Nova::Gsi_flags(Nova::Gsi_flags::HIGH);
		else
			return Nova::Gsi_flags(Nova::Gsi_flags::LOW);
	};

	_gsi_flags = gsi_flags(irq_args);

	/* associate GSI or MSI to device belonging to device_phys */
	if (irq_args.msi()) {
		if (!associate_msi(irq_sel(), device_phys, _msi_addr, _msi_data, _sigh_cap)) {
			error("unable to associate IRQ");
			return;
		}
	} else {
		if (!associate_gsi(irq_sel(), _sigh_cap, _gsi_flags)) {
			error("unable to associate GSI");
			return;
		}
	}

	_device_phys = device_phys;
}


Irq_object::Irq_object()
:
	_kernel_caps(cap_map().insert(KERNEL_CAP_COUNT_LOG2)),
	_msi_addr(0UL), _msi_data(0UL)
{ }


Irq_object::~Irq_object()
{
	if (_sigh_cap.valid())
		deassociate(irq_sel());

	/* revoke IRQ SM */
	Nova::revoke(Nova::Obj_crd(_kernel_caps, KERNEL_CAP_COUNT_LOG2));
	enum { NO_REVOKE_REQUIRED = false };
	cap_map().remove(_kernel_caps, KERNEL_CAP_COUNT_LOG2, NO_REVOKE_REQUIRED);
}


/***************************
 ** IRQ session component **
 ***************************/

static Range_allocator::Result allocate(Range_allocator &irq_alloc, Irq_args const &args)
{
	unsigned n = unsigned(args.irq_number());

	if (args.type() != Irq_session::TYPE_LEGACY) {

		if (n >= kernel_hip().sel_gsi)
			return Alloc_error::DENIED;

		n = kernel_hip().sel_gsi - 1 - n;
		/* XXX last GSI number unknown - assume 40 GSIs (depends on IO-APIC) */
		if (n < 40)
			return Alloc_error::DENIED;
	}

	return irq_alloc.alloc_addr(1, n);
}


Irq_session_component::Irq_session_component(Runtime &,
                                             Range_allocator &irq_alloc,
                                             const char      *args)
:
	_irq_number(allocate(irq_alloc, Irq_args(args))), _irq_object()
{
	_irq_number.with_result(
		[&] (Range_allocator::Allocation const &a) {
			long device_phys = Arg_string::find_arg(args, "device_config_phys").long_value(0);
			_irq_object.start(unsigned(addr_t(a.ptr)), device_phys, Irq_args(args));
		},
		[&] (Alloc_error) {
			error("unavailable IRQ ", Irq_args(args).irq_number(), " requested"); });
}


Irq_session_component::~Irq_session_component() { }


void Irq_session_component::ack_irq()
{
	_irq_object.ack_irq();
}


void Irq_session_component::sigh(Signal_context_capability cap)
{
	_irq_object.sigh(cap);
}


Irq_session::Info Irq_session_component::info()
{
	if (!_irq_object.msi_address() || !_irq_object.msi_value())
		return { };

	return {
		.type    = Info::Type::MSI,
		.address = _irq_object.msi_address(),
		.value   = _irq_object.msi_value()
	};
}
