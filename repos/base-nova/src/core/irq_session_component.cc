/*
 * \brief  Implementation of IRQ session component
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \author Alexander Boettcher
 * \date   2009-10-02
 */

/*
 * Copyright (C) 2009-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <base/sleep.h>

/* core includes */
#include <irq_root.h>
#include <platform.h>
#include <platform_pd.h>

/* NOVA includes */
#include <nova/syscalls.h>
#include <nova/util.h>
#include <nova_util.h>

using namespace Genode;


static void thread_start()
{
	Thread_base::myself()->entry();
	sleep_forever();
}


static bool associate(unsigned irq, Genode::addr_t irq_sel,
                      Genode::addr_t &msi_addr, Genode::addr_t &msi_data,
                      Genode::addr_t virt_addr = 0)
{
	/* map IRQ SM cap from kernel to core at irq_sel selector */
	using Nova::Obj_crd;

	Obj_crd src(platform_specific()->gsi_base_sel() + irq, 0);
	Obj_crd dst(irq_sel, 0);
	enum { MAP_FROM_KERNEL_TO_CORE = true };

	int ret = map_local((Nova::Utcb *)Thread_base::myself()->utcb(),
	                    src, dst, MAP_FROM_KERNEL_TO_CORE);
	if (ret) {
		PERR("Could not map IRQ %u", irq);
		return false;
	}

	/* assign IRQ to CPU && request msi data to be used by driver */
	uint8_t res = Nova::assign_gsi(irq_sel, virt_addr, boot_cpu(),
	                               msi_addr, msi_data);

	if (virt_addr && res != Nova::NOVA_OK) {
		PERR("setting up MSI %u failed - error %u", irq, res);
		return false;
	}

	/* nova syscall interface specifies msi addr/data to be 32bit */
	msi_addr = msi_addr & ~0U;
	msi_data = msi_data & ~0U;

	return true;
}


static bool msi(unsigned irq, Genode::addr_t irq_sel, Genode::addr_t phys_mem,
                Genode::addr_t &msi_addr, Genode::addr_t &msi_data)
{
	void * virt = 0;
	if (platform()->region_alloc()->alloc_aligned(4096, &virt, 12).is_error())
		return false;

	Genode::addr_t virt_addr = reinterpret_cast<Genode::addr_t>(virt);
	if (!virt_addr)
		return false;

	using Nova::Rights;
	using Nova::Utcb;

	Nova::Mem_crd phys_crd(phys_mem >> 12,  0, Rights(true, false, false));
	Nova::Mem_crd virt_crd(virt_addr >> 12, 0, Rights(true, false, false));
	Utcb * utcb = reinterpret_cast<Utcb *>(Thread_base::myself()->utcb());

	if (map_local_phys_to_virt(utcb, phys_crd, virt_crd)) {
		platform()->region_alloc()->free(virt, 4096);
		return false;
	}

	/* try to assign MSI to device */
	bool res = associate(irq, irq_sel, msi_addr, msi_data, virt_addr);

	unmap_local(Nova::Mem_crd(virt_addr >> 12, 0, Rights(true, true, true)));
	platform()->region_alloc()->free(virt, 4096);

	return res;
}


void Irq_object::start()
{
	PERR("wrong start method called");
	throw Root::Unavailable();
}


/**
 * Create global EC, associate it to SC
 */
void Irq_object::start(unsigned irq, Genode::addr_t const device_phys)
{
	/* associate GSI or MSI to device belonging to device_phys */
	bool ok = false;
	if (device_phys)
		ok = msi(irq, irq_sel(), device_phys, _msi_addr, _msi_data);
	else
		ok = associate(irq, irq_sel(), _msi_addr, _msi_data);

	if (!ok)
		throw Root::Unavailable();

	/* start thread having a SC */
	using namespace Nova;
	addr_t pd_sel = Platform_pd::pd_core_sel();
	addr_t utcb   = reinterpret_cast<addr_t>(&_context->utcb);

	/* put IP on stack, it will be read from core pager in platform.cc */
	addr_t *sp = reinterpret_cast<addr_t *>(_context->stack_top() - sizeof(addr_t));
	*sp        = reinterpret_cast<addr_t>(thread_start);

	/* create global EC */
	enum { GLOBAL = true };
	uint8_t res = create_ec(_tid.ec_sel, pd_sel, boot_cpu(),
	                        utcb, (mword_t)sp, _tid.exc_pt_sel, GLOBAL);
	if (res != NOVA_OK) {
		PERR("%p - create_ec returned %d", this, res);
		throw Root::Unavailable();
	}

	/* remap startup portal from main thread */
	if (map_local((Utcb *)Thread_base::myself()->utcb(),
	               Obj_crd(PT_SEL_STARTUP, 0),
	               Obj_crd(_tid.exc_pt_sel + PT_SEL_STARTUP, 0))) {
		PERR("could not create startup portal");
		throw Root::Unavailable();
	}

	/* remap debugging page fault portal for core threads */
	if (map_local((Utcb *)Thread_base::myself()->utcb(),
	               Obj_crd(PT_SEL_PAGE_FAULT, 0),
	               Obj_crd(_tid.exc_pt_sel + PT_SEL_PAGE_FAULT, 0))) {
		PERR("could not create page fault portal");
		throw Root::Unavailable();
	}

	/* default: we don't accept any mappings or translations */
	Utcb * utcb_obj = reinterpret_cast<Utcb *>(Thread_base::utcb());
	utcb_obj->crd_rcv = Obj_crd();
	utcb_obj->crd_xlt = Obj_crd();

	/* create SC */
	Qpd qpd(Qpd::DEFAULT_QUANTUM, Qpd::DEFAULT_PRIORITY + 1);
	res = create_sc(sc_sel(), pd_sel, _tid.ec_sel, qpd);
	if (res != NOVA_OK) {
		PERR("%p - create_sc returned returned %d", this, res);
		throw Root::Unavailable();
	}

	_sync_life.lock();
}


void Irq_object::entry()
{
	/* signal that thread is up and ready */
	_sync_life.unlock();

	/* wait for first ack_irq */
	_sync_ack.lock();

	while (true) {

		if (Nova::NOVA_OK != Nova::sm_ctrl(irq_sel(), Nova::SEMAPHORE_DOWN))
			PERR("Error: blocking for irq_sel 0x%lx failed", irq_sel());

		if (_state == SHUTDOWN) {
			/* signal end of life to entrypoint thread */
			_sync_life.unlock();
			while (1) nova_die();
		}

		if (!_sig_cap.valid())
			continue;

		notify();

		_sync_ack.lock();
	}
}


Irq_object::Irq_object()
:
	Thread<4096>("irq"),
	_sync_ack(Lock::LOCKED), _sync_life(Lock::LOCKED),
	_kernel_caps(cap_map()->insert(KERNEL_CAP_COUNT_LOG2)),
	_msi_addr(0UL), _msi_data(0UL), _state(UNDEFINED)
{ }


Irq_object::~Irq_object()
{
	/* tell interrupt thread to get in a defined dead state */
	_state = SHUTDOWN;
	/* send ack - thread maybe got not the first ack */
	_sync_ack.unlock();
	/* unblock thread if it is waiting for interrupts */
	Nova::sm_ctrl(irq_sel(), Nova::SEMAPHORE_UP);
	/* wait until thread signals end of life */
	_sync_life.lock();

	/* revoke SC and SM of interrupt source */
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
	_irq_number(~0U), _irq_alloc(irq_alloc)
{
	long irq_number = Arg_string::find_arg(args, "irq_number").long_value(-1);
	long device_phys = Arg_string::find_arg(args, "device_config_phys").long_value(0);
	if (device_phys) {

		if (irq_number >= kernel_hip()->sel_gsi)
			throw Root::Unavailable();

		irq_number = kernel_hip()->sel_gsi - 1 - irq_number;
		/* XXX last GSI number unknown - assume 40 GSIs (depends on IO-APIC) */
		if (irq_number < 40)
			throw Root::Unavailable();
	}

	if (!irq_alloc || irq_alloc->alloc_addr(1, irq_number).is_error()) {
		PERR("Unavailable IRQ 0x%lx requested", irq_number);
		throw Root::Unavailable();
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
		return { .type = Genode::Irq_session::Info::Type::INVALID };

	return {
		.type    = Genode::Irq_session::Info::Type::MSI,
		.address = _irq_object.msi_address(),
		.value   = _irq_object.msi_value()
	};
}
