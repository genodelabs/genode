/*
 * \brief  Kernel backend for execution contexts in userland
 * \author Martin Stein
 * \date   2013-09-15
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <kernel/thread.h>
#include <kernel/pd.h>
#include <platform_thread.h>
#include <platform_pd.h>

using namespace Kernel;


char const * Kernel::Thread::label()
{
	if (!platform_thread()) {
		if (!phys_utcb()) { return "idle"; }
		return "core";
	}
	return platform_thread()->name();
}


char const * Kernel::Thread::pd_label()
{
	if (core()) { return "core"; }
	if (!pd()) { return "?"; }
	return pd()->platform_pd()->label();
}


void
Kernel::Thread::prepare_to_start(void * const        ip,
                                 void * const        sp,
                                 unsigned const      cpu_id,
                                 unsigned const      pd_id,
                                 Native_utcb * const utcb_phys,
                                 Native_utcb * const utcb_virt,
                                 bool const          main)
{
	assert(_state == AWAIT_START)

	/* FIXME: support SMP */
	if (cpu_id) { PERR("multicore processing not supported"); }

	/* store thread parameters */
	_phys_utcb = utcb_phys;
	_virt_utcb = utcb_virt;
	_pd_id     = pd_id;

	/* join a protection domain */
	Pd * const pd = Pd::pool()->object(_pd_id);
	assert(pd);
	addr_t const tlb = pd->tlb()->base();

	/* initialize CPU context */
	User_context * const c = static_cast<User_context *>(this);
	bool const core = (_pd_id == core_id());
	if (!main) { c->init_thread(ip, sp, tlb, pd_id); }
	else if (!core) { c->init_main_thread(ip, utcb_virt, tlb, pd_id); }
	else { c->init_core_main_thread(ip, sp, tlb, pd_id); }

	/* print log message */
	if (START_VERBOSE) {
		PINF("in program %u '%s' start thread %u '%s'",
		      this->pd_id(), pd_label(), id(), label());
	}
}


Kernel::Thread::Thread(Platform_thread * const platform_thread)
: _platform_thread(platform_thread), _state(AWAIT_START),
  _pager(0), _pd_id(0), _phys_utcb(0), _virt_utcb(0),
  _signal_receiver(0)
{
	priority = _platform_thread ? _platform_thread->priority()
	                            : Kernel::Priority::MAX;
}
