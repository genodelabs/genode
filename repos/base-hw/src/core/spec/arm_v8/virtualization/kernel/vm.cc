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

#include <cpu/vcpu_state_virtualization.h>
#include <util/mmio.h>

#include <hw/assert.h>
#include <hw/spec/arm_64/memory_map.h>
#include <map_local.h>
#include <platform_pd.h>
#include <kernel/cpu.h>
#include <kernel/vm.h>
#include <kernel/main.h>

#include <spec/arm_v8/virtualization/hypervisor.h>

using Genode::addr_t;
using Kernel::Cpu;
using Kernel::Vm;


static Genode::Vcpu_state & host_context(Cpu & cpu)
{
	static Genode::Constructible<Genode::Vcpu_state>
		host_context[Board::NR_OF_CPUS];

	if (!host_context[cpu.id()].constructed()) {
		host_context[cpu.id()].construct();
		Genode::Vcpu_state & c = *host_context[cpu.id()];
		c.sp_el1    = cpu.stack_start();
		c.ip        = (addr_t)&Kernel::main_handle_kernel_entry;
		c.pstate    = 0;
		Cpu::Spsr::Sp::set(c.pstate, 1); /* select non-el0 stack pointer */
		Cpu::Spsr::El::set(c.pstate, Cpu::Current_el::EL1);
		Cpu::Spsr::F::set(c.pstate, 1);
		Cpu::Spsr::I::set(c.pstate, 1);
		Cpu::Spsr::A::set(c.pstate, 1);
		Cpu::Spsr::D::set(c.pstate, 1);
		c.fpcr      = Cpu::Fpcr::read();
		c.fpsr      = 0;
		c.sctlr_el1 = Cpu::Sctlr_el1::read();
		c.actlr_el1 = Cpu::Actlr_el1::read();
		c.vbar_el1  = Cpu::Vbar_el1::read();
		c.cpacr_el1 = Cpu::Cpacr_el1::read();
		c.ttbr0_el1 = Cpu::Ttbr0_el1::read();
		c.ttbr1_el1 = Cpu::Ttbr1_el1::read();
		c.tcr_el1   = Cpu::Tcr_el1::read();
		c.mair_el1  = Cpu::Mair_el1::read();
		c.amair_el1 = Cpu::Amair_el1::read();
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
	Vm *vm = dynamic_cast<Vm*>(&_cpu.current_context());
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
	asm volatile("msr cntv_ctl_el0, xzr");
	asm volatile("msr cntkctl_el1,  %0" :: "r" (0b11));
}


Vm::Vm(Irq::Pool              & user_irq_pool,
       Cpu                    & cpu,
       Genode::Vcpu_data      & data,
       Kernel::Signal_context & context,
       Identity               & id)
:
	Kernel::Object { *this },
	Cpu_context(cpu, Scheduler::Priority::min(), 0),
	_user_irq_pool(user_irq_pool),
	_state(data),
	_context(context),
	_id(id),
	_vcpu_context(cpu)
{
	_state.id_aa64isar0_el1 = Cpu::Id_aa64isar0_el1::read();
	_state.id_aa64isar1_el1 = Cpu::Id_aa64isar1_el1::read();
	_state.id_aa64mmfr0_el1 = Cpu::Id_aa64mmfr0_el1::read();
	_state.id_aa64mmfr1_el1 = Cpu::Id_aa64mmfr1_el1::read();
	_state.id_aa64mmfr2_el1 = /* FIXME Cpu::Id_aa64mmfr2_el1::read(); */ 0;

	Cpu::Clidr_el1::access_t clidr = Cpu::Clidr_el1::read();
	for (unsigned i = 0; i < 7; i++) {
		unsigned level = clidr >> (i*3) & 0b111;

		if (level == Cpu::Clidr_el1::NO_CACHE) break;

		if ((level == Cpu::Clidr_el1::INSTRUCTION_CACHE) ||
		    (level == Cpu::Clidr_el1::SEPARATE_CACHE)) {
			Cpu::Csselr_el1::access_t csselr = 0;
			Cpu::Csselr_el1::Instr::set(csselr, 1);
			Cpu::Csselr_el1::Level::set(csselr, level);
			Cpu::Csselr_el1::write(csselr);
			_state.ccsidr_inst_el1[level] = Cpu::Ccsidr_el1::read();
		}

		if (level != Cpu::Clidr_el1::INSTRUCTION_CACHE) {
			Cpu::Csselr_el1::write(Cpu::Csselr_el1::Level::bits(level));
			_state.ccsidr_data_el1[level] = Cpu::Ccsidr_el1::read();
		}
	}

	/* once constructed, exit with a startup exception */
	pause();
	_state.exception_type = Genode::VCPU_EXCEPTION_STARTUP;
	_context.submit(1);
}


Vm::~Vm()
{
	Cpu::Vttbr_el2::access_t vttbr_el2 =
		Cpu::Vttbr_el2::Ba::masked((Cpu::Vttbr_el2::access_t)_id.table);
	Cpu::Vttbr_el2::Asid::set(vttbr_el2, _id.id);
	Hypervisor::invalidate_tlb(vttbr_el2);
}


void Vm::exception()
{
	switch (_state.exception_type) {
	case Cpu::IRQ_LEVEL_EL0: [[fallthrough]];
	case Cpu::IRQ_LEVEL_EL1: [[fallthrough]];
	case Cpu::FIQ_LEVEL_EL0: [[fallthrough]];
	case Cpu::FIQ_LEVEL_EL1:
		_interrupt(_user_irq_pool);
		break;
	case Cpu::SYNC_LEVEL_EL0: [[fallthrough]];
	case Cpu::SYNC_LEVEL_EL1: [[fallthrough]];
	case Cpu::SERR_LEVEL_EL0: [[fallthrough]];
	case Cpu::SERR_LEVEL_EL1:
		pause();
		_context.submit(1);
		break;
	default:
		Genode::raw("Exception vector: ", (void*)_state.exception_type,
		            " not implemented!");
	};

	if (_cpu().pic().ack_virtual_irq(_vcpu_context.pic))
		inject_irq(Board::VT_MAINTAINANCE_IRQ);
	_vcpu_context.vtimer_irq.disable();
}


void Vm::proceed()
{
	if (_state.timer.irq) _vcpu_context.vtimer_irq.enable();

	_cpu().pic().insert_virtual_irq(_vcpu_context.pic, _state.irqs.virtual_irq);

	/*
	 * the following values have to be enforced by the hypervisor
	 */
	Cpu::Vttbr_el2::access_t vttbr_el2 =
		Cpu::Vttbr_el2::Ba::masked((Cpu::Vttbr_el2::access_t)_id.table);
	Cpu::Vttbr_el2::Asid::set(vttbr_el2, _id.id);
	addr_t guest = Hw::Mm::el2_addr(&_state);
	addr_t pic   = Hw::Mm::el2_addr(&_vcpu_context.pic);
	addr_t host  = Hw::Mm::el2_addr(&host_context(_cpu()));

	Hypervisor::switch_world(guest, host, pic, vttbr_el2);
}


void Vm::run()
{
	_sync_from_vmm();
	if (_scheduled != ACTIVE) Cpu_context::_activate();
	_scheduled = ACTIVE;
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
