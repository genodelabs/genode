/*
 * \brief  Kernel backend for virtual machines
 * \author Stefan Kalkowski
 * \author Benjamin Lamowski
 * \date   2015-02-10
 */

/*
 * Copyright (C) 2015-2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <hw/assert.h>
#include <cpu/vcpu_state_virtualization.h>

#include <platform_pd.h>
#include <kernel/cpu.h>
#include <kernel/vm.h>
#include <kernel/main.h>
#include <spec/arm_v7/virtualization/hypervisor.h>

namespace Kernel {

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
}

using namespace Kernel;


struct Hypervisor::Host_context
{
	Cpu::Ttbr_64bit::access_t vttbr;
	Cpu::Hcr::access_t        hcr;
	Cpu::Hstr::access_t       hstr;
	Cpu::Cpacr::access_t      cpacr;
	addr_t                    sp;
	addr_t                    ip;
	addr_t                    spsr;
	Cpu::Ttbr_64bit::access_t ttbr0;
	Cpu::Ttbr_64bit::access_t ttbr1;
	Cpu::Sctlr::access_t      sctlr;
	Cpu::Ttbcr::access_t      ttbcr;
	Cpu::Mair0::access_t      mair0;
	Cpu::Dacr::access_t       dacr;
	Cpu::Vmpidr::access_t     vmpidr;

} vt_host_context;


static Hypervisor::Host_context & host_context(Cpu & cpu)
{
	static Genode::Constructible<Hypervisor::Host_context>
		host_context[Board::NR_OF_CPUS];
	if (!host_context[cpu.id()].constructed()) {
		host_context[cpu.id()].construct();
		Hypervisor::Host_context & c = *host_context[cpu.id()];
		c.sp     = cpu.stack_start();
		c.ttbr0  = Cpu::Ttbr0_64bit::read();
		c.ttbr1  = Cpu::Ttbr1_64bit::read();
		c.sctlr  = Cpu::Sctlr::read();
		c.ttbcr  = Cpu::Ttbcr::read();
		c.mair0  = Cpu::Mair0::read();
		c.dacr   = Cpu::Dacr::read();
		c.vmpidr = Cpu::Mpidr::read();
		c.ip     = (addr_t)&Kernel::main_handle_kernel_entry;
		c.vttbr  = 0;
		c.hcr    = 0;
		c.hstr   = 0;
		c.cpacr  = 0xf00000;
		c.spsr   = 0x1d3;
	}
	return *host_context[cpu.id()];
}


Board::Vcpu_context::Vm_irq::Vm_irq(unsigned const irq, Cpu & cpu)
:
	Kernel::Irq { irq, cpu.irq_pool(), cpu.pic() },
	_cpu        { cpu }
{ }


void Board::Vcpu_context::Vm_irq::handle(Vm & vm, unsigned irq) {
	vm.inject_irq(irq); }


void Board::Vcpu_context::Vm_irq::occurred()
{
	Vm *vm = dynamic_cast<Vm*>(&_cpu.scheduled_job());
	if (!vm) Genode::raw("VM interrupt while VM is not runnning!");
	else     handle(*vm, _irq_nr);
}


Board::Vcpu_context::Pic_maintainance_irq::Pic_maintainance_irq(Cpu & cpu)
:
	Board::Vcpu_context::Vm_irq(Board::VT_MAINTAINANCE_IRQ, cpu)
{
	//FIXME Irq::enable only enables caller cpu
	cpu.pic().unmask(_irq_nr, cpu.id());
}


Board::Vcpu_context::Virtual_timer_irq::Virtual_timer_irq(Cpu & cpu)
:
	irq(Board::VT_TIMER_IRQ, cpu)
{ }


void Board::Vcpu_context::Virtual_timer_irq::enable() { irq.enable(); }


void Board::Vcpu_context::Virtual_timer_irq::disable()
{
	irq.disable();
	asm volatile("mcr p15, 0, %0, c14, c3, 1" :: "r" (0));
	asm volatile("mcr p15, 0, %0, c14, c1, 0" :: "r" (0b11));
}


Kernel::Vm::Vm(Irq::Pool              & user_irq_pool,
               Cpu                    & cpu,
               Genode::Vcpu_data      & data,
               Kernel::Signal_context & context,
               Identity               & id)
:
	Kernel::Object { *this },
	Cpu_job(Cpu_priority::min(), 0),
	_user_irq_pool(user_irq_pool),
	_state(data),
	_context(context),
	_id(id),
	_vcpu_context(cpu)
{
	affinity(cpu);
	/* once constructed, exit with a startup exception */
	pause();
	_state.cpu_exception = Genode::VCPU_EXCEPTION_STARTUP;
	_context.submit(1);
}


Kernel::Vm::~Vm()
{
	Cpu::Ttbr_64bit::access_t vttbr =
		Cpu::Ttbr_64bit::Ba::masked((Cpu::Ttbr_64bit::access_t)_id.table);
	Cpu::Ttbr_64bit::Asid::set(vttbr, _id.id);
	Hypervisor::invalidate_tlb(vttbr);
}


void Kernel::Vm::exception(Cpu & cpu)
{
	switch(_state.cpu_exception) {
	case Genode::Cpu_state::INTERRUPT_REQUEST:
	case Genode::Cpu_state::FAST_INTERRUPT_REQUEST:
		_interrupt(_user_irq_pool, cpu.id());
		break;
	default:
		pause();
		_context.submit(1);
	}

	if (cpu.pic().ack_virtual_irq(_vcpu_context.pic))
		inject_irq(Board::VT_MAINTAINANCE_IRQ);
	_vcpu_context.vtimer_irq.disable();
}


void Kernel::Vm::proceed(Cpu & cpu)
{
	if (_state.timer.irq) _vcpu_context.vtimer_irq.enable();

	cpu.pic().insert_virtual_irq(_vcpu_context.pic, _state.irqs.virtual_irq);

	/*
	 * the following values have to be enforced by the hypervisor
	 */
	_state.vttbr = Cpu::Ttbr_64bit::Ba::masked((Cpu::Ttbr_64bit::access_t)_id.table);
	Cpu::Ttbr_64bit::Asid::set(_state.vttbr, _id.id);

	/*
	 * use the following report fields not needed for loading the context
	 * to transport the HSTR and HCR register descriptions into the assembler
	 * path in a dense way
	 */
	_state.esr_el2   = Cpu::Hstr::init();
	_state.hpfar_el2 = Cpu::Hcr::init();

	Hypervisor::switch_world(_state, host_context(cpu));
}


void Vm::_sync_to_vmm()
{}


void Vm::_sync_from_vmm()
{}


void Vm::inject_irq(unsigned irq)
{
	_state.irqs.last_irq = irq;
	pause();
	_context.submit(1);
}
