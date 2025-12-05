/*
 * \brief   Class for kernel data that is needed to manage a specific CPU
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2014-01-14
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <cpu/consts.h>

/* core includes */
#include <kernel/cpu.h>
#include <kernel/thread.h>
#include <kernel/irq.h>
#include <kernel/pd.h>
#include <board.h>
#include <kernel/assert.h>
#include <hw/boot_info.h>
#include <hw/memory_consts.h>

/* for backtrace */
#include <util/string.h>
namespace Genode {
	void for_each_return_address(Const_byte_range_ptr const &, auto const &);
}
#include <os/for_each_return_address.h>

using namespace Kernel;


/*****************
 ** Cpu_context **
 *****************/

void Cpu_context::_activate() { _cpu().assign(*this); }


void Cpu_context::_deactivate()
{
	ASSERT(!remotely_running());
	_cpu().scheduler().unready(*this);
}


void Cpu_context::_yield()
{
	ASSERT(_cpu().id() == Cpu::executing_id());
	_cpu().scheduler().yield();
}


void Cpu_context::_interrupt()
{
	/* let the IRQ controller take a pending IRQ for handling, if any */
	unsigned irq_id;
	if (_cpu().pic().take_request(irq_id))

		/*
		 * let the CPU of this context handle the IRQ,
		 * if it is a CPU-local one, otherwise use the global pool
		 */
		if (!_cpu().handle_if_cpu_local_interrupt(irq_id))
			_cpu()._pool.irq_pool().with(irq_id,
				[] (Irq &irq) { irq.occurred(); },
				[&] { Genode::raw("Unknown interrupt ", irq_id); });

	/* let the IRQ controller finish the currently taken IRQ */
	_cpu().pic().finish_request();
}


bool Cpu_context::remotely_running()
{
	return (_cpu().id() != Cpu::executing_id()) &&
	       _cpu().scheduler().current_helping_destination(*this);
}


Cpu_context::Cpu_context(Cpu &cpu, Group_id const id)
:
	Context(id), _cpu_ptr(&cpu) { }


Cpu_context::~Cpu_context()
{
	_deactivate();
}


/*********
 ** Cpu **
 *********/

void Cpu::assign(Context &context)
{
	_scheduler.ready(static_cast<Scheduler::Context&>(context));
	if (_id != executing_id()) trigger_ip_interrupt();
}


void Cpu::backtrace()
{
	using namespace Genode;

	log("");
	log("Backtrace of kernel context on cpu ", _id.value, ":");

	Const_byte_range_ptr const stack {
		(char const*)stack_base(), Hw::Mm::KERNEL_STACK_SIZE };
	for_each_return_address(stack, [&] (void **p) { log(*p); });
}


bool Cpu::handle_if_cpu_local_interrupt(unsigned const irq_id)
{
	bool found = false;

	Irq::Pool::with(irq_id,
		[&] (auto &irq) {
			irq.occurred();
			found = true;
		}, [] { });

	return found;
}


Cpu::Context & Cpu::schedule_next_context()
{
	if (_state == SUSPEND || _state == HALT)
		return _halt_job;

	_scheduler.update();
	return current_context();
}


addr_t Cpu::stack_base()
{
	return Hw::Mm::cpu_local_memory().base +
	       Hw::Mm::CPU_LOCAL_MEMORY_SLOT_SIZE * _id.value;
}


addr_t Cpu::stack_start()
{
	return Abi::stack_align(stack_base() + Hw::Mm::KERNEL_STACK_SIZE);
}


Cpu::Cpu(Id const id, Cpu_pool &cpu_pool, Pd &core_pd)
:
	_pool      { cpu_pool },
	_id        { id },
	_pic       { cpu_pool._global_irq_ctrl },
	_timer     { *this },
	_idle      { *this, core_pd },
	_scheduler { _timer, _idle },
	_ipi_irq   { *this }
{
	_arch_init();

	/*
	 * We insert the cpu objects in order into the cpu_pool's list
	 * to ensure that the cpu with the lowest given id is the first
	 * one.
	 */
	Cpu * cpu = cpu_pool._cpus.first();
	while (cpu && cpu->next() && (cpu->next()->id() < _id))
		cpu = cpu->next();
	cpu = (cpu && cpu->id() < _id) ? cpu : nullptr;
	cpu_pool._cpus.insert(this, cpu);
}


/**************
 ** Cpu_pool **
 **************/

template <typename T>
static inline T* cpu_object_by_id(Cpu::Id const id)
{
	using namespace Hw::Mm;
	addr_t base = CPU_LOCAL_MEMORY_AREA_START +
	              id.value * CPU_LOCAL_MEMORY_SLOT_SIZE;
	return (T*)(base + CPU_LOCAL_MEMORY_SLOT_OBJECT_OFFSET);
}


void Cpu_pool::initialize_executing_cpu(Pd &core_pd)
{
	Cpu::Id id = Cpu::executing_id();
	Genode::construct_at<Cpu>(cpu_object_by_id<void>(id), id, *this, core_pd);
}


Cpu & Cpu_pool::cpu(Cpu::Id const id)
{
	return *cpu_object_by_id<Cpu>(id);
}
