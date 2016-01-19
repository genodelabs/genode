/*
 * \brief   Class for kernel data that is needed to manage a specific CPU
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2014-01-14
 */

/*
 * Copyright (C) 2014-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <kernel/cpu.h>
#include <kernel/kernel.h>
#include <kernel/thread.h>
#include <kernel/irq.h>
#include <kernel/pd.h>
#include <pic.h>
#include <timer.h>
#include <assert.h>

/* base includes */
#include <unmanaged_singleton.h>

using namespace Kernel;

namespace Kernel
{
	Cpu_pool * cpu_pool() { return unmanaged_singleton<Cpu_pool>(); }
}


/*************
 ** Cpu_job **
 *************/

void Cpu_job::_activate_own_share() { _cpu->schedule(this); }


void Cpu_job::_deactivate_own_share()
{
	assert(_cpu->id() == Cpu::executing_id());
	_cpu->scheduler()->unready(this);
}


void Cpu_job::_yield()
{
	assert(_cpu->id() == Cpu::executing_id());
	_cpu->scheduler()->yield();
}


void Cpu_job::_interrupt(unsigned const cpu_id)
{
	/* determine handling for specific interrupt */
	unsigned irq_id;
	if (pic()->take_request(irq_id))

		/* is the interrupt a cpu-local one */
		if (!_cpu->interrupt(irq_id)) {

			/* it needs to be a user interrupt */
			User_irq * irq = User_irq::object(irq_id);
			if (irq) irq->occurred();
			else PWRN("Unknown interrupt %u", irq_id);
		}

	/* end interrupt request at controller */
	pic()->finish_request();
}


void Cpu_job::affinity(Cpu * const cpu)
{
	_cpu = cpu;
	_cpu->scheduler()->insert(this);
}


void Cpu_job::quota(unsigned const q)
{
	if (_cpu) { _cpu->scheduler()->quota(this, q); }
	else { Cpu_share::quota(q); }
}


Cpu_job::Cpu_job(Cpu_priority const p, unsigned const q)
: Cpu_share(p, q), _cpu(0) { }


Cpu_job::~Cpu_job()
{
	if (!_cpu) { return; }
	_cpu->scheduler()->remove(this);
}


/**************
 ** Cpu_idle **
 **************/

void Cpu_idle::proceed(unsigned const cpu) { mtc()->switch_to_user(this, cpu); }


void Cpu_idle::_main() { while (1) { Genode::Cpu::wait_for_interrupt(); } }


/*********
 ** Cpu **
 *********/

void Cpu::schedule(Job * const job)
{
	if (_id == executing_id()) { _scheduler.ready(job); }
	else if (_scheduler.ready_check(job)) { trigger_ip_interrupt(); }
}


bool Cpu::interrupt(unsigned const irq_id)
{
	Irq * const irq = object(irq_id);
	if (!irq) return false;
	irq->occurred();
	return true;
}


Cpu_job & Cpu::schedule()
{
	/* get new job */
	Job & old_job = scheduled_job();

	/* update scheduler */
	unsigned const old_time = _scheduler.head_quota();
	unsigned const new_time = _timer->value(_id);
	unsigned quota = old_time > new_time ? old_time - new_time : 1;
	_scheduler.update(quota);

	/* get new job */
	Job & new_job = scheduled_job();
	quota = _scheduler.head_quota();
	assert(quota);
	_timer->start_one_shot(quota, _id);

	/* switch to new job */
	switch_to(new_job);

	/* return new job */
	return new_job;
}


Cpu::Cpu(unsigned const id, Timer * const timer)
: _id(id), _idle(this), _timer(timer),
  _scheduler(&_idle, _quota(), _fill()),
  _ipi_irq(*this),
  _timer_irq(_timer->interrupt_id(_id), *this) { }


/**************
 ** Cpu_pool **
 **************/

Cpu * Cpu_pool::cpu(unsigned const id) const
{
	assert(id < NR_OF_CPUS);
	char * const p = const_cast<char *>(_cpus[id]);
	return reinterpret_cast<Cpu *>(p);
}


Cpu_pool::Cpu_pool()
{
	for (unsigned id = 0; id < NR_OF_CPUS; id++) {
		new (_cpus[id]) Cpu(id, &_timer); }
}


/***********************
 ** Cpu_domain_update **
 ***********************/

Cpu_domain_update::Cpu_domain_update() {
	for (unsigned i = 0; i < NR_OF_CPUS; i++) { _pending[i] = false; } }


/*****************
 ** Cpu_context **
 *****************/

/**
 * Enable kernel-entry assembly to get an exclusive stack for every CPU
 */
enum { KERNEL_STACK_SIZE = 64 * 1024 };
Genode::size_t  kernel_stack_size = KERNEL_STACK_SIZE;
Genode::uint8_t kernel_stack[NR_OF_CPUS][KERNEL_STACK_SIZE]
__attribute__((aligned(16)));

Cpu_context::Cpu_context(Genode::Translation_table * const table)
{
	sp = (addr_t)kernel_stack;
	ip = (addr_t)kernel;
	core_pd()->admit(this);

	/*
	 * platform specific initialization, has to be done after
	 * setting the registers by now
	 */
	_init(KERNEL_STACK_SIZE, (addr_t)table);
}
