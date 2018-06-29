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

/* Genode includes */
#include <base/log.h>

/* core includes */
#include <irq_root.h>
#include <platform.h>

/* NOVA includes */
#include <nova/syscalls.h>
#include <nova_util.h>

using namespace Genode;


static bool irq_ctrl(Genode::addr_t irq_sel,
                     Genode::addr_t &msi_addr, Genode::addr_t &msi_data,
                     Genode::addr_t sig_sel, Genode::addr_t virt_addr = 0)
{
	/* assign IRQ to CPU && request msi data to be used by driver */
	uint8_t res = Nova::assign_gsi(irq_sel, virt_addr, boot_cpu(),
	                               msi_addr, msi_data, sig_sel);

	if (res != Nova::NOVA_OK)
		error("setting up MSI failed - error ", res);

	/* nova syscall interface specifies msi addr/data to be 32bit */
	msi_addr = msi_addr & ~0U;
	msi_data = msi_data & ~0U;

	return res == Nova::NOVA_OK;
}


static bool associate(Genode::addr_t irq_sel,
                      Genode::addr_t &msi_addr, Genode::addr_t &msi_data,
                      Genode::Signal_context_capability sig_cap,
                      Genode::addr_t virt_addr = 0)
{
	return irq_ctrl(irq_sel, msi_addr, msi_data, sig_cap.local_name(),
	                virt_addr);
}


static void deassociate(Genode::addr_t irq_sel)
{
	addr_t dummy1 = 0, dummy2 = 0;

	if (!irq_ctrl(irq_sel, dummy1, dummy2, irq_sel))
		warning("Irq could not be de-associated");
}


static bool msi(Genode::addr_t irq_sel, Genode::addr_t phys_mem,
                Genode::addr_t &msi_addr, Genode::addr_t &msi_data,
                Genode::Signal_context_capability sig_cap)
{
	void * virt = 0;
	if (platform()->region_alloc()->alloc_aligned(4096, &virt, 12).error())
		return false;

	Genode::addr_t virt_addr = reinterpret_cast<Genode::addr_t>(virt);
	if (!virt_addr)
		return false;

	using Nova::Rights;
	using Nova::Utcb;

	Nova::Mem_crd phys_crd(phys_mem >> 12,  0, Rights(true, false, false));
	Nova::Mem_crd virt_crd(virt_addr >> 12, 0, Rights(true, false, false));
	Utcb * utcb = reinterpret_cast<Utcb *>(Thread::myself()->utcb());

	if (map_local_phys_to_virt(utcb, phys_crd, virt_crd, platform_specific()->core_pd_sel())) {
		platform()->region_alloc()->free(virt, 4096);
		return false;
	}

	/* try to assign MSI to device */
	bool res = associate(irq_sel, msi_addr, msi_data, sig_cap, virt_addr);

	unmap_local(Nova::Mem_crd(virt_addr >> 12, 0, Rights(true, true, true)));
	platform()->region_alloc()->free(virt, 4096);

	return res;
}


void Irq_object::sigh(Signal_context_capability cap)
{
	if (!_sigh_cap.valid() && !cap.valid())
		return;

	if ((_sigh_cap.valid() && !cap.valid())) {
		deassociate(irq_sel());
		_sigh_cap = Signal_context_capability();
		return;
	}

	bool ok = false;
	if (_device_phys)
		ok = msi(irq_sel(), _device_phys, _msi_addr, _msi_data, cap);
	else
	    ok = associate(irq_sel(), _msi_addr, _msi_data, cap);

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


void Irq_object::start(unsigned irq, Genode::addr_t const device_phys)
{
	/* map IRQ SM cap from kernel to core at irq_sel selector */
	using Nova::Obj_crd;

	Obj_crd src(platform_specific()->gsi_base_sel() + irq, 0);
	Obj_crd dst(irq_sel(), 0);
	enum { MAP_FROM_KERNEL_TO_CORE = true };

	int ret = map_local(platform_specific()->core_pd_sel(),
	                    (Nova::Utcb *)Thread::myself()->utcb(),
	                    src, dst, MAP_FROM_KERNEL_TO_CORE);
	if (ret) {
		error("getting IRQ from kernel failed - ", irq);
		throw Service_denied();
	}

	/* associate GSI or MSI to device belonging to device_phys */
	bool ok = false;
	if (device_phys)
		ok = msi(irq_sel(), device_phys, _msi_addr, _msi_data, _sigh_cap);
	else
		ok = associate(irq_sel(), _msi_addr, _msi_data, _sigh_cap);

	if (!ok)
		throw Service_denied();

	_device_phys = device_phys;
}


Irq_object::Irq_object()
:
	_kernel_caps(cap_map()->insert(KERNEL_CAP_COUNT_LOG2)),
	_msi_addr(0UL), _msi_data(0UL)
{ }


Irq_object::~Irq_object()
{
	if (_sigh_cap.valid())
		deassociate(irq_sel());

	/* revoke IRQ SM */
	Nova::revoke(Nova::Obj_crd(_kernel_caps, KERNEL_CAP_COUNT_LOG2));
	enum { NO_REVOKE_REQUIRED = false };
	cap_map()->remove(_kernel_caps, KERNEL_CAP_COUNT_LOG2, NO_REVOKE_REQUIRED);
}


/***************************
 ** IRQ session component **
 ***************************/


static Nova::Hip * kernel_hip()
{
	/**
	 * Initial value of esp register, saved by the crt0 startup code.
	 * This value contains the address of the hypervisor information page.
	 */
	extern addr_t __initial_sp;
	return reinterpret_cast<Nova::Hip *>(__initial_sp);
}


Irq_session_component::Irq_session_component(Range_allocator *irq_alloc,
                                             const char      *args)
:
	_irq_number(~0U), _irq_alloc(irq_alloc), _irq_object()
{
	long irq_number = Arg_string::find_arg(args, "irq_number").long_value(-1);
	long device_phys = Arg_string::find_arg(args, "device_config_phys").long_value(0);
	if (device_phys) {

		if ((unsigned long)irq_number >= kernel_hip()->sel_gsi)
			throw Service_denied();

		irq_number = kernel_hip()->sel_gsi - 1 - irq_number;
		/* XXX last GSI number unknown - assume 40 GSIs (depends on IO-APIC) */
		if (irq_number < 40)
			throw Service_denied();
	}

	if (!irq_alloc || irq_alloc->alloc_addr(1, irq_number).error()) {
		error("unavailable IRQ ", irq_number, " requested");
		throw Service_denied();
	}

	_irq_number = irq_number;

	_irq_object.start(_irq_number, device_phys);
}


Irq_session_component::~Irq_session_component()
{
	if (_irq_number == ~0U)
		return;

	Genode::addr_t free_irq = _irq_number;
	_irq_alloc->free((void *)free_irq);
}


void Irq_session_component::ack_irq()
{
	_irq_object.ack_irq();
}


void Irq_session_component::sigh(Genode::Signal_context_capability cap)
{
	_irq_object.sigh(cap);
}


Genode::Irq_session::Info Irq_session_component::info()
{
	if (!_irq_object.msi_address() || !_irq_object.msi_value())
		return { .type = Info::Type::INVALID, .address = 0, .value = 0 };

	return {
		.type    = Info::Type::MSI,
		.address = _irq_object.msi_address(),
		.value   = _irq_object.msi_value()
	};
}
