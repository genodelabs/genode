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
#include <hw/assert.h>
#include <hw/boot_info.h>
#include <hw/memory_consts.h>

using namespace Kernel;


/*************
 ** Cpu_job **
 *************/

void Cpu_job::_activate_own_share() { _cpu->schedule(this); }


void Cpu_job::_deactivate_own_share()
{
	assert(_cpu->id() == Cpu::executing_id());
	_cpu->scheduler().unready(*this);
}


void Cpu_job::_yield()
{
	assert(_cpu->id() == Cpu::executing_id());
	_cpu->scheduler().yield();
}


void Cpu_job::_interrupt(Irq::Pool &user_irq_pool, unsigned const /* cpu_id */)
{
	/* let the IRQ controller take a pending IRQ for handling, if any */
	unsigned irq_id;
	if (_cpu->pic().take_request(irq_id))

		/* let the CPU of this job handle the IRQ if it is a CPU-local one */
		if (!_cpu->handle_if_cpu_local_interrupt(irq_id)) {

			/* it isn't a CPU-local IRQ, so, it must be a user IRQ */
			User_irq * irq = User_irq::object(user_irq_pool, irq_id);
			if (irq) irq->occurred();
			else Genode::raw("Unknown interrupt ", irq_id);
		}

	/* let the IRQ controller finish the currently taken IRQ */
	_cpu->pic().finish_request();
}


void Cpu_job::affinity(Cpu &cpu)
{
	_cpu = &cpu;
	_cpu->scheduler().insert(*this);
}


void Cpu_job::quota(unsigned const q)
{
	if (_cpu)
		_cpu->scheduler().quota(*this, q);
	else
		Context::quota(q);
}


Cpu_job::Cpu_job(Priority const p, unsigned const q)
:
	Context(p, q), _cpu(0)
{ }


Cpu_job::~Cpu_job()
{
	if (!_cpu)
		return;

	_cpu->scheduler().remove(*this);
}


/*********
 ** Cpu **
 *********/

extern "C" void idle_thread_main(void);


Cpu::Idle_thread::Idle_thread(Board::Address_space_id_allocator &addr_space_id_alloc,
                              Irq::Pool                         &user_irq_pool,
                              Cpu_pool                          &cpu_pool,
                              Cpu                               &cpu,
                              Pd                                &core_pd)
:
	Thread { addr_space_id_alloc, user_irq_pool, cpu_pool, core_pd,
	         Priority::min(), 0, "idle", Thread::IDLE }
{
	regs->ip = (addr_t)&idle_thread_main;

	affinity(cpu);
	Thread::_pd = &core_pd;
}


void Cpu::schedule(Job * const job)
{
	_scheduler.ready(job->context());
	if (_id != executing_id() && _scheduler.need_to_schedule())
		trigger_ip_interrupt();
}


bool Cpu::handle_if_cpu_local_interrupt(unsigned const irq_id)
{
	Irq * const irq = object(irq_id);

	if (!irq)
		return false;

	irq->occurred();
	return true;
}


Cpu_job & Cpu::schedule()
{
	/* update scheduler */
	Job & old_job = scheduled_job();
	old_job.exception(*this);

	if (_state == SUSPEND || _state == HALT)
		return _halt_job;

	if (_scheduler.need_to_schedule()) {
		_timer.process_timeouts();
		_scheduler.update(_timer.time());
		time_t t = _scheduler.current_time_left();
		_timer.set_timeout(&_timeout, t);
		time_t duration = _timer.schedule_timeout();
		old_job.update_execution_time(duration);
	}

	/* return new job */
	return scheduled_job();
}


addr_t Cpu::stack_start()
{
	return Abi::stack_align(Hw::Mm::cpu_local_memory().base +
	                        (1024*1024*_id) + (64*1024));
}


Cpu::Cpu(unsigned                     const  id,
         Board::Address_space_id_allocator  &addr_space_id_alloc,
         Irq::Pool                          &user_irq_pool,
         Cpu_pool                           &cpu_pool,
         Pd                                 &core_pd,
         Board::Global_interrupt_controller &global_irq_ctrl)
:
	_id               { id },
	_pic              { global_irq_ctrl },
	_timer            { *this },
	_scheduler        { _idle, _quota(), _fill() },
	_idle             { addr_space_id_alloc, user_irq_pool, cpu_pool, *this,
	                    core_pd },
	_ipi_irq          { *this },
	_global_work_list { cpu_pool.work_list() }
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
static inline T* cpu_object_by_id(unsigned const id)
{
	using namespace Hw::Mm;
	addr_t base = CPU_LOCAL_MEMORY_AREA_START + id*CPU_LOCAL_MEMORY_SLOT_SIZE;
	return (T*)(base + CPU_LOCAL_MEMORY_SLOT_OBJECT_OFFSET);
}


void
Cpu_pool::
initialize_executing_cpu(Board::Address_space_id_allocator  &addr_space_id_alloc,
                         Irq::Pool                          &user_irq_pool,
                         Pd                                 &core_pd,
                         Board::Global_interrupt_controller &global_irq_ctrl)
{
	unsigned id = Cpu::executing_id();
	Genode::construct_at<Cpu>(cpu_object_by_id<void>(id), id,
	                          addr_space_id_alloc, user_irq_pool,
	                          *this, core_pd, global_irq_ctrl);
}


Cpu & Cpu_pool::cpu(unsigned const id)
{
	return *cpu_object_by_id<Cpu>(id);
}
