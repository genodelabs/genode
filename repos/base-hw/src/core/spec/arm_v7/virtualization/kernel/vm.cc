/*
 * \brief   Kernel backend for virtual machines
 * \author  Stefan Kalkowski
 * \date    2015-02-10
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <platform_pd.h>
#include <kernel/vm.h>

namespace Kernel
{
	Vm_pool * vm_pool() { return unmanaged_singleton<Vm_pool>(); }

	/**
	 * ARM's virtual interrupt controller cpu interface
	 */
	struct Virtual_pic;

	/**
	 * ARM's virtual timer counter
	 */
	struct Virtual_timer;

	/**
	 * Kernel private virtualization interrupts, delivered to VM/VMMs
	 */
	struct Vm_irq;

	/**
	 * Cpu-specific initialization for virtualization support
	 */
	void prepare_hypervisor(void);
}

using namespace Kernel;

extern void *         _vt_vm_entry;
extern void *         _vt_host_entry;
extern Genode::addr_t _vt_vm_context_ptr;
extern Genode::addr_t _vt_host_context_ptr;
extern Genode::addr_t _mt_master_context_begin;


struct Kernel::Vm_irq : Kernel::Irq
{
		Vm_irq(unsigned const irq) : Kernel::Irq(irq, *cpu_pool()->executing_cpu()) {}

	/**
	 * A VM interrupt gets injected into the VM scheduled on the current CPU
	 */
	void occurred()
	{
		Cpu_job * job = cpu_pool()->executing_cpu()->scheduled_job();
		Vm *vm = dynamic_cast<Vm*>(job);
		if (!vm)
			PERR("VM timer interrupt while VM is not runnning!");
		else
			vm->inject_irq(_id());
	}
};


struct Kernel::Virtual_pic : Genode::Mmio
{
	struct Gich_hcr    : Register<0x00, 32> { };
	struct Gich_vmcr   : Register<0x08, 32> { };
	struct Gich_misr   : Register<0x10, 32> { };
	struct Gich_eisr0  : Register<0x20, 32> { };
	struct Gich_elrsr0 : Register<0x30, 32> { };
	struct Gich_apr    : Register<0xf0, 32> { };

	template <unsigned SLOT>
	struct Gich_lr : Register<0x100 + SLOT*4, 32> { };

	Vm_irq irq = Genode::Board::VT_MAINTAINANCE_IRQ;

	Virtual_pic()
	: Genode::Mmio(Genode::Board::IRQ_CONTROLLER_VT_CTRL_BASE) { }

	static Virtual_pic& pic()
	{
		static Virtual_pic vgic;
		return vgic;
	}

	/**
	 * Save the virtual interrupt controller state to VM state
	 */
	static void save (Genode::Vm_state *s)
	{
		s->gic_hcr    = pic().read<Gich_hcr   >();
		s->gic_misr   = pic().read<Gich_misr  >();
		s->gic_vmcr   = pic().read<Gich_vmcr  >();
		s->gic_apr    = pic().read<Gich_apr   >();
		s->gic_eisr   = pic().read<Gich_eisr0 >();
		s->gic_elrsr0 = pic().read<Gich_elrsr0>();
		s->gic_lr[0]  = pic().read<Gich_lr<0> >();
		s->gic_lr[1]  = pic().read<Gich_lr<1> >();
		s->gic_lr[2]  = pic().read<Gich_lr<2> >();
		s->gic_lr[3]  = pic().read<Gich_lr<3> >();

		/* disable virtual PIC CPU interface */
		pic().write<Gich_hcr>(0);
	}

	/**
	 * Load the virtual interrupt controller state from VM state
	 */
	static void load (Genode::Vm_state *s)
	{
		pic().write<Gich_hcr   >(s->gic_hcr );
		pic().write<Gich_misr  >(s->gic_misr);
		pic().write<Gich_vmcr  >(s->gic_vmcr);
		pic().write<Gich_apr   >(s->gic_apr );
		pic().write<Gich_elrsr0>(s->gic_elrsr0);
		pic().write<Gich_lr<0> >(s->gic_lr[0]);
		pic().write<Gich_lr<1> >(s->gic_lr[1]);
		pic().write<Gich_lr<2> >(s->gic_lr[2]);
		pic().write<Gich_lr<3> >(s->gic_lr[3]);
	}
};


struct Kernel::Virtual_timer
{
	Vm_irq irq = Genode::Board::VT_TIMER_IRQ;

	/**
	 * Return virtual timer object of currently executing cpu
	 *
	 * FIXME: remove this when re-designing the CPU (issue #1252)
	 */
	static Virtual_timer& timer()
	{
		static Virtual_timer timer[NR_OF_CPUS];
		return timer[Cpu::executing_id()];
	}

	/**
	 * Resets the virtual timer, thereby it disables its interrupt
	 */
	static void reset()
	{
		timer().irq.disable();
		asm volatile("mcr p15, 0, %0, c14, c3, 1 \n"
		             "mcr p15, 0, %0, c14, c3, 0" :: "r" (0));
	}

	/**
	 * Save the virtual timer state to VM state
	 */
	static void save(Genode::Vm_state *s)
	{
		asm volatile("mrc p15, 0, %0, c14, c3, 0 \n"
		             "mrc p15, 0, %1, c14, c3, 1" :
		             "=r" (s->timer_val), "=r" (s->timer_ctrl));
	}

	/**
	 * Load the virtual timer state from VM state
	 */
	static void load(Genode::Vm_state *s, unsigned const cpu_id)
	{
		if (s->timer_irq) timer().irq.enable();

		asm volatile("mcr p15, 0, %0, c14, c3, 1 \n"
		             "mcr p15, 0, %1, c14, c3, 0 \n"
		             "mcr p15, 0, %2, c14, c3, 1" ::
		             "r" (0),
		             "r" (s->timer_val), "r" (s->timer_ctrl));
	}
};


void Kernel::prepare_hypervisor()
{
	/* set hypervisor exception vector */
	Cpu::hyp_exception_entry_at(&_vt_host_entry);

	/* set hypervisor's translation table */
	Genode::Translation_table * table =
		core_pd()->platform_pd()->translation_table_phys();
	Cpu::Httbr::translation_table((addr_t)table);

	/* prepare MMU usage by hypervisor code */
	Cpu::Htcr::write(Cpu::Ttbcr::init_virt_kernel());
	Cpu::Hcptr::write(Cpu::Hcptr::init());
	Cpu::Hmair0::write(Cpu::Mair0::init_virt_kernel());
	Cpu::Vtcr::write(Cpu::Vtcr::init());
	Cpu::Hsctlr::write(Cpu::Sctlr::init_virt_kernel());

	/* initialize host context used in virtualization world switch */
	*((void**)&_vt_host_context_ptr) = &_mt_master_context_begin;
}


Kernel::Vm::Vm(void                   * const state,
               Kernel::Signal_context * const context,
               void                   * const table)
:  Cpu_job(Cpu_priority::min, 0),
  _state((Genode::Vm_state * const)state),
  _context(context),
  _table(table) {
	affinity(cpu_pool()->primary_cpu());
	Virtual_pic::pic().irq.enable();
}


void Kernel::Vm::exception(unsigned const cpu_id)
{
	Virtual_timer::save(_state);

	switch(_state->cpu_exception) {
	case Genode::Cpu_state::INTERRUPT_REQUEST:
	case Genode::Cpu_state::FAST_INTERRUPT_REQUEST:
		_interrupt(cpu_id);
		break;
	default:
		pause();
		_context->submit(1);
	}

	Virtual_pic::save(_state);
	Virtual_timer::reset();
}


void Kernel::Vm::proceed(unsigned const cpu_id)
{
	/*
	 * the following values have to be enforced by the hypervisor
	 */
	_state->vttbr = Cpu::Ttbr0::init((Genode::addr_t)_table, id());

	/*
	 * use the following report fields not needed for loading the context
	 * to transport the HSTR and HCR register descriptions into the assembler
	 * path in a dense way
	 */
	_state->hsr   = Cpu::Hstr::init();
	_state->hpfar = Cpu::Hcr::init();

	Virtual_pic::load(_state);
	Virtual_timer::load(_state, cpu_id);

	mtc()->switch_to(reinterpret_cast<Cpu::Context*>(_state), cpu_id,
	                 (addr_t) &_vt_vm_entry, (addr_t)&_vt_vm_context_ptr);
}


void Vm::inject_irq(unsigned irq)
{
	_state->gic_irq = irq;
	pause();
	_context->submit(1);
}
