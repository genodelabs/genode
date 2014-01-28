/*
 * \brief  NOVA-specific implementation of the Thread API for core
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \author Alexander Boettcher
 * \date   2010-01-19
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/thread.h>
#include <base/printf.h>

/* NOVA includes */
#include <nova/syscalls.h>

/* core includes */
#include <nova_util.h>
#include <platform_pd.h>

using namespace Genode;


void Thread_base::_init_platform_thread(Type type)
{
	/*
	 * This function is called for constructing server activations and pager
	 * objects. It allocates capability selectors for the thread's execution
	 * context and a synchronization-helper semaphore needed for 'Lock'.
	 */
	using namespace Nova;

	if (type == MAIN)
	{
		/* set EC selector according to NOVA spec */
		_tid.ec_sel = Platform_pd::pd_core_sel() + 1;

		/*
		 * Exception base of first thread in core is 0. We have to set
		 * it here so that Thread_base code finds the semaphore of the
		 * main thread.
		 */
		_tid.exc_pt_sel = 0;

		return;
	}
	_tid.ec_sel     = cap_map()->insert(1);
	_tid.exc_pt_sel = cap_map()->insert(NUM_INITIAL_PT_LOG2);
	addr_t pd_sel   = Platform_pd::pd_core_sel();

	/* create running semaphore required for locking */
	addr_t rs_sel =_tid.exc_pt_sel + SM_SEL_EC;
	uint8_t res = create_sm(rs_sel, pd_sel, 0);
	if (res != NOVA_OK) {
		PERR("create_sm returned %u", res);
		throw Cpu_session::Thread_creation_failed();
	}
}


void Thread_base::_deinit_platform_thread()
{
	unmap_local(Nova::Obj_crd(_tid.ec_sel, 1));
	unmap_local(Nova::Obj_crd(_tid.exc_pt_sel, Nova::NUM_INITIAL_PT_LOG2));

	cap_map()->remove(_tid.ec_sel, 1, false);
	cap_map()->remove(_tid.exc_pt_sel, Nova::NUM_INITIAL_PT_LOG2, false);

	/* revoke utcb */
	Nova::Rights rwx(true, true, true);
	addr_t utcb = reinterpret_cast<addr_t>(&_context->utcb);
	Nova::revoke(Nova::Mem_crd(utcb >> 12, 0, rwx));
}


void Thread_base::start()
{
	/*
	 * On NOVA, core almost never starts regular threads. This simply creates a
	 * local EC
	 */
	using namespace Nova;

	addr_t sp       = _context->stack_top();
	addr_t utcb     = reinterpret_cast<addr_t>(&_context->utcb);
	Utcb * utcb_obj = reinterpret_cast<Utcb *>(&_context->utcb);
	addr_t pd_sel   = Platform_pd::pd_core_sel();

	/*
	 * In core, the affinity location was write to the stack base by the server
	 * code. So, thry to read the value from there.
	 */
	Affinity::Location location = reinterpret_cast<Affinity::Location *>(stack_base())[0];

	if (!location.valid())
		location = Affinity::Location(boot_cpu(), 0);

	/* create local EC */
	enum { LOCAL_THREAD = false };
	uint8_t res = create_ec(_tid.ec_sel, pd_sel, location.xpos(),
	                        utcb, sp, _tid.exc_pt_sel, LOCAL_THREAD);
	if (res != NOVA_OK) {
		PERR("create_ec returned %d", res);
		throw Cpu_session::Thread_creation_failed();
	}

	/* default: we don't accept any mappings or translations */
	utcb_obj->crd_rcv = Obj_crd();
	utcb_obj->crd_xlt = Obj_crd();

	if (map_local(reinterpret_cast<Nova::Utcb *>(Thread_base::myself()->utcb()),
	              Obj_crd(PT_SEL_PAGE_FAULT, 0),
	              Obj_crd(_tid.exc_pt_sel + PT_SEL_PAGE_FAULT, 0))) {
		PERR("could not create page fault portal");
		throw Cpu_session::Thread_creation_failed();
	}
}


void Thread_base::cancel_blocking()
{
	using namespace Nova;

	if (sm_ctrl(_tid.exc_pt_sel + SM_SEL_EC, SEMAPHORE_UP))
		nova_die();
}
