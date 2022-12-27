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

/* core includes */
#include <kernel/cpu.h>
#include <kernel/thread.h>
#include <kernel/irq.h>
#include <kernel/pd.h>
#include <board.h>
#include <hw/assert.h>
#include <hw/boot_info.h>

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
		Cpu_share::quota(q);
}


Cpu_job::Cpu_job(Cpu_priority const p, unsigned const q)
:
	Cpu_share(p, q), _cpu(0)
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
	         Cpu_priority::min(), 0, "idle", Thread::IDLE }
{
	regs->ip = (addr_t)&idle_thread_main;

	affinity(cpu);
	Thread::_pd = &core_pd;
}


void Cpu::schedule(Job * const job)
{
	_scheduler.ready(job->share());
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

	if (_scheduler.need_to_schedule()) {
		_timer.process_timeouts();
		_scheduler.update(_timer.time());
		time_t t = _scheduler.head_quota();
		_timer.set_timeout(this, t);
		time_t duration = _timer.schedule_timeout();
		old_job.update_execution_time(duration);
	}

	/* return new job */
	return scheduled_job();
}


Genode::size_t  kernel_stack_size = Cpu::KERNEL_STACK_SIZE;
Genode::uint8_t kernel_stack[NR_OF_CPUS][Cpu::KERNEL_STACK_SIZE]
__attribute__((aligned(Genode::get_page_size())));


addr_t Cpu::stack_start()
{
	return (addr_t)&kernel_stack + KERNEL_STACK_SIZE * (_id + 1);
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
}


/**************
 ** Cpu_pool **
 **************/

void
Cpu_pool::
initialize_executing_cpu(Board::Address_space_id_allocator  &addr_space_id_alloc,
                         Irq::Pool                          &user_irq_pool,
                         Pd                                 &core_pd,
                         Board::Global_interrupt_controller &global_irq_ctrl)
{
	unsigned id = Cpu::executing_id();
	_cpus[id].construct(
		id, addr_space_id_alloc, user_irq_pool, *this, core_pd, global_irq_ctrl);
}


Cpu & Cpu_pool::cpu(unsigned const id)
{
	assert(id < _nr_of_cpus && _cpus[id].constructed());
	return *_cpus[id];
}


using Boot_info = Hw::Boot_info<Board::Boot_info>;


Cpu_pool::Cpu_pool(unsigned nr_of_cpus)
:
	_nr_of_cpus(nr_of_cpus)
{ }
