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
#include <kernel/kernel.h>
#include <kernel/thread.h>
#include <kernel/irq.h>
#include <kernel/pd.h>
#include <pic.h>
#include <hw/assert.h>

/* base-internal includes */
#include <base/internal/unmanaged_singleton.h>

using namespace Kernel;

namespace Kernel
{
	Cpu_pool * cpu_pool() { return unmanaged_singleton<Cpu_pool>(); }
}


/*************
 ** Cpu_job **
 *************/

time_t Cpu_job::timeout_age_us(Timeout const * const timeout) const
{
	return _cpu->timeout_age_us(timeout);
}


time_t Cpu_job::time() const { return _cpu->time(); }


time_t Cpu_job::timeout_max_us() const
{
	return _cpu->timeout_max_us();
}


void Cpu_job::timeout(Timeout * const timeout, time_t const us)
{
	_cpu->set_timeout(timeout, us);
}


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
			else Genode::warning("Unknown interrupt ", irq_id);
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
:
	Cpu_share(p, q), _cpu(0) { }


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

void Cpu::set_timeout(Timeout * const timeout, time_t const duration_us) {
	_timer.set_timeout(timeout, _timer.us_to_ticks(duration_us)); }


time_t Cpu::timeout_age_us(Timeout const * const timeout) const {
	return _timer.timeout_age_us(timeout); }


time_t Cpu::timeout_max_us() const { return _timer.timeout_max_us(); }


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
	/* update scheduler */
	time_t quota = _timer.update_time();
	Job & old_job = scheduled_job();
	old_job.exception(id());
	_timer.process_timeouts();
	_scheduler.update(quota);

	/* get new job */
	Job & new_job = scheduled_job();
	quota = _scheduler.head_quota();

	_timer.set_timeout(this, quota);

	_timer.schedule_timeout();

	/* switch to new job */
	switch_to(new_job);

	/* return new job */
	return new_job;
}


Cpu::Cpu(unsigned const id)
:
	_id(id), _timer(_id), _idle(this),
	_scheduler(&_idle, _quota(), _fill()),
	_ipi_irq(*this), _timer_irq(_timer.interrupt_id(), *this)
{ }


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
		new (_cpus[id]) Cpu(id); }
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
 * FIXME THIS IS ONLY USED BY IDLE THREAD
 * Enable kernel-entry assembly to get an exclusive stack for every CPU
 *
 * The stack alignment is determined as follows:
 *
 * 1) There is an architectural minimum alignment for stacks that originates
 *    from the assumptions that some instructions make.
 * 2) Shared cache lines between yet uncached and already cached
 *    CPUs during multiprocessor bring-up must be avoided. Thus, the alignment
 *    must be at least the maximum line size of global caches.
 * 3) The alignment that originates from 1) and 2) is assumed to be always
 *    less or equal to the minimum page size.
 */
enum { KERNEL_STACK_SIZE = 16 * 1024 * sizeof(Genode::addr_t) };
Genode::size_t  kernel_stack_size = KERNEL_STACK_SIZE;
Genode::uint8_t kernel_stack[NR_OF_CPUS][KERNEL_STACK_SIZE]
__attribute__((aligned(Genode::get_page_size())));

Cpu_context::Cpu_context(Hw::Page_table * const table)
{
	sp = (addr_t)kernel_stack;
	ip = (addr_t)kernel;

	/*
	 * platform specific initialization, has to be done after
	 * setting the registers by now
	 */
	_init(KERNEL_STACK_SIZE, (addr_t)table);
}
