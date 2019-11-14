/*
 * \brief   Kernel backend for virtual machines
 * \author  Stefan Kalkowski
 * \date    2015-02-10
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/log.h>
#include <hw/assert.h>
#include <cpu/vm_state_virtualization.h>

#include <platform_pd.h>
#include <kernel/cpu.h>
#include <kernel/vm.h>

namespace Kernel
{
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

extern "C" void   kernel();
extern     void * kernel_stack;
extern "C" void   hypervisor_enter_vm(Genode::Vm_state&);

struct Host_context {
	addr_t                    sp;
	addr_t                    ip;
	Cpu::Ttbr_64bit::access_t ttbr0;
	Cpu::Ttbr_64bit::access_t ttbr1;
	Cpu::Sctlr::access_t      sctlr;
	Cpu::Ttbcr::access_t      ttbcr;
	Cpu::Mair0::access_t      mair0;
	Cpu::Dacr::access_t       dacr;
} vt_host_context;


Board::Vcpu_context::Vm_irq::Vm_irq(unsigned const irq, Cpu & cpu)
: Kernel::Irq(irq, cpu.irq_pool())
{ }


void Board::Vcpu_context::Vm_irq::handle(Cpu &, Vm & vm, unsigned irq) {
	vm.inject_irq(irq); }


void Board::Vcpu_context::Vm_irq::occurred()
{
	Cpu & cpu = Kernel::cpu_pool().executing_cpu();
	Vm *vm = dynamic_cast<Vm*>(&cpu.scheduled_job());
	if (!vm) Genode::raw("VM interrupt while VM is not runnning!");
	else     handle(cpu, *vm, _irq_nr);
}


Board::Vcpu_context::Pic_maintainance_irq::Pic_maintainance_irq(Cpu & cpu)
: Board::Vcpu_context::Vm_irq(Board::VT_MAINTAINANCE_IRQ, cpu) {
	//FIXME Irq::enable only enables caller cpu
	cpu.pic().unmask(_irq_nr, cpu.id()); }

Board::Vcpu_context::Virtual_timer_irq::Virtual_timer_irq(Cpu & cpu)
: irq(Board::VT_TIMER_IRQ, cpu) {}


void Board::Vcpu_context::Virtual_timer_irq::enable() { irq.enable(); }


void Board::Vcpu_context::Virtual_timer_irq::disable()
{
	irq.disable();
	asm volatile("mcr p15, 0, %0, c14, c3, 1" :: "r" (0));
	asm volatile("mcr p15, 0, %0, c14, c1, 0" :: "r" (0b11));
}

using Vmid_allocator = Genode::Bit_allocator<256>;

static Vmid_allocator &alloc()
{
	static Vmid_allocator * allocator = nullptr;
	if (!allocator) {
		allocator = unmanaged_singleton<Vmid_allocator>();

		/* reserve VM ID 0 for the hypervisor */
		unsigned id = allocator->alloc();
		assert (id == 0);
	}
	return *allocator;
}


Kernel::Vm::Vm(unsigned, /* FIXME: smp support */
               Genode::Vm_state       & state,
               Kernel::Signal_context & context,
               void                   * const table)
:  Cpu_job(Cpu_priority::MIN, 0),
  _id(alloc().alloc()),
  _state(state),
  _context(context),
  _table(table),
  _vcpu_context(cpu_pool().primary_cpu())
{
	affinity(cpu_pool().primary_cpu());

	vt_host_context.sp    = _cpu->stack_start();
	vt_host_context.ttbr0 = Cpu::Ttbr0_64bit::read();
	vt_host_context.ttbr1 = Cpu::Ttbr1_64bit::read();
	vt_host_context.sctlr = Cpu::Sctlr::read();
	vt_host_context.ttbcr = Cpu::Ttbcr::read();
	vt_host_context.mair0 = Cpu::Mair0::read();
	vt_host_context.dacr  = Cpu::Dacr::read();
	vt_host_context.ip    = (addr_t) &kernel;
}


Kernel::Vm::~Vm() { alloc().free(_id); }


void Kernel::Vm::exception(Cpu & cpu)
{
	switch(_state.cpu_exception) {
	case Genode::Cpu_state::INTERRUPT_REQUEST:
	case Genode::Cpu_state::FAST_INTERRUPT_REQUEST:
		_interrupt(cpu.id());
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
	_state.vttbr = Cpu::Ttbr_64bit::Ba::masked((Cpu::Ttbr_64bit::access_t)_table);
	Cpu::Ttbr_64bit::Asid::set(_state.vttbr, _id);

	/*
	 * use the following report fields not needed for loading the context
	 * to transport the HSTR and HCR register descriptions into the assembler
	 * path in a dense way
	 */
	_state.esr_el2   = Cpu::Hstr::init();
	_state.hpfar_el2 = Cpu::Hcr::init();

	hypervisor_enter_vm(_state);
}


void Vm::inject_irq(unsigned irq)
{
	_state.irqs.last_irq = irq;
	pause();
	_context.submit(1);
}
