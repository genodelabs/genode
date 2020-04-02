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
#include <cpu/vm_state_virtualization.h>
#include <util/mmio.h>

#include <hw/assert.h>
#include <hw/spec/arm_64/memory_map.h>
#include <map_local.h>
#include <platform_pd.h>
#include <kernel/cpu.h>
#include <kernel/vm.h>

using Genode::addr_t;
using Kernel::Cpu;
using Kernel::Vm;

extern "C" void   kernel();
extern     void * kernel_stack;
extern "C" void   hypervisor_enter_vm(addr_t vm, addr_t host,
                                      addr_t pic, addr_t guest_table,
                                      bool invalidate_tlb_vm);


static Genode::Vm_state & host_context()
{
	static Genode::Constructible<Genode::Vm_state> host_context;
	if (!host_context.constructed()) {
		host_context.construct();
		host_context->ip        = (addr_t) &kernel;
		host_context->pstate    = 0;
		Cpu::Spsr::Sp::set(host_context->pstate, 1); /* select non-el0 stack pointer */
		Cpu::Spsr::El::set(host_context->pstate, Cpu::Current_el::EL1);
		Cpu::Spsr::F::set(host_context->pstate, 1);
		Cpu::Spsr::I::set(host_context->pstate, 1);
		Cpu::Spsr::A::set(host_context->pstate, 1);
		Cpu::Spsr::D::set(host_context->pstate, 1);
		host_context->fpcr      = Cpu::Fpcr::read();
		host_context->fpsr      = 0;
		host_context->sctlr_el1 = Cpu::Sctlr_el1::read();
		host_context->actlr_el1 = Cpu::Actlr_el1::read();
		host_context->vbar_el1  = Cpu::Vbar_el1::read();
		host_context->cpacr_el1 = Cpu::Cpacr_el1::read();
		host_context->ttbr0_el1 = Cpu::Ttbr0_el1::read();
		host_context->ttbr1_el1 = Cpu::Ttbr1_el1::read();
		host_context->tcr_el1   = Cpu::Tcr_el1::read();
		host_context->mair_el1  = Cpu::Mair_el1::read();
		host_context->amair_el1 = Cpu::Amair_el1::read();
	}
	return *host_context;
}


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
	asm volatile("msr cntv_ctl_el0, xzr");
	asm volatile("msr cntkctl_el1,  %0" :: "r" (0b11));
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


Vm::Vm(unsigned                 cpu,
       Genode::Vm_state       & state,
       Kernel::Signal_context & context,
       void                   * const table)
:  Cpu_job(Cpu_priority::MIN, 0),
  _id(alloc().alloc()),
  _state(state),
  _context(context),
  _table(table),
  _vcpu_context(cpu_pool().cpu(cpu))
{
	affinity(cpu_pool().cpu(cpu));

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
}


Vm::~Vm() { alloc().free(_id); }


void Vm::exception(Cpu & cpu)
{
	switch (_state.exception_type) {
	case Cpu::IRQ_LEVEL_EL0: [[fallthrough]]
	case Cpu::IRQ_LEVEL_EL1: [[fallthrough]]
	case Cpu::FIQ_LEVEL_EL0: [[fallthrough]]
	case Cpu::FIQ_LEVEL_EL1:
		_interrupt(cpu.id());
		break;
	case Cpu::SYNC_LEVEL_EL0: [[fallthrough]]
	case Cpu::SYNC_LEVEL_EL1: [[fallthrough]]
	case Cpu::SERR_LEVEL_EL0: [[fallthrough]]
	case Cpu::SERR_LEVEL_EL1:
		pause();
		_context.submit(1);
		break;
	default:
		Genode::raw("Exception vector: ", (void*)_state.exception_type,
		            " not implemented!");
	};

	if (cpu.pic().ack_virtual_irq(_vcpu_context.pic))
		inject_irq(Board::VT_MAINTAINANCE_IRQ);
	_vcpu_context.vtimer_irq.disable();
}


void Vm::proceed(Cpu & cpu)
{
	if (_state.timer.irq) _vcpu_context.vtimer_irq.enable();

	cpu.pic().insert_virtual_irq(_vcpu_context.pic, _state.irqs.virtual_irq);

	/*
	 * the following values have to be enforced by the hypervisor
	 */
	Cpu::Vttbr_el2::access_t vttbr_el2 =
		Cpu::Vttbr_el2::Ba::masked((Cpu::Vttbr_el2::access_t)_table);
	Cpu::Vttbr_el2::Asid::set(vttbr_el2, _id);
	addr_t guest = Hw::Mm::el2_addr(&_state);
	addr_t pic   = Hw::Mm::el2_addr(&_vcpu_context.pic);
	addr_t host  = Hw::Mm::el2_addr(&host_context());
	host_context().sp_el1 = cpu.stack_start();

	/* check and reset invalidation flag */
    bool inval_tlb_vm = false;
	if (_inval_tlb_vm) {
        inval_tlb_vm = true;
        _inval_tlb_vm = false;
	}

	hypervisor_enter_vm(guest, host, pic, vttbr_el2, inval_tlb_vm);
}

void Vm::inject_irq(unsigned irq)
{
	_state.irqs.last_irq = irq;
	pause();
	_context.submit(1);
}

bool Vm::_inval_tlb_vm = false;

void Vm::invalidate_tlb_vm() {
    /*
     * set invalidation flag; invalidation is done by the hypervisor
     */
    _inval_tlb_vm = true;
}
