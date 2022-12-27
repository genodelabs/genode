/*
 * \brief  NOVA-specific implementation of the Thread API for core
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \author Alexander Boettcher
 * \date   2010-01-19
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/thread.h>
#include <base/log.h>

/* base-internal includes */
#include <base/internal/stack.h>

/* NOVA includes */
#include <nova/syscalls.h>

/* core includes */
#include <platform.h>
#include <nova_util.h>
#include <trace/source_registry.h>

using namespace Genode;


void Thread::_init_platform_thread(size_t, Type type)
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
		native_thread().ec_sel = platform_specific().core_pd_sel() + 1;

		/*
		 * Exception base of first thread in core is 0. We have to set
		 * it here so that Thread code finds the semaphore of the
		 * main thread.
		 */
		native_thread().exc_pt_sel = 0;

		return;
	}
	native_thread().ec_sel     = cap_map().insert(1);
	native_thread().exc_pt_sel = cap_map().insert(NUM_INITIAL_PT_LOG2);

	/* create running semaphore required for locking */
	addr_t rs_sel =native_thread().exc_pt_sel + SM_SEL_EC;
	uint8_t res = create_sm(rs_sel, platform_specific().core_pd_sel(), 0);
	if (res != NOVA_OK) {
		error("create_sm returned ", res);
		throw Cpu_session::Thread_creation_failed();
	}
}


void Thread::_deinit_platform_thread()
{
	unmap_local(Nova::Obj_crd(native_thread().ec_sel, 1));
	unmap_local(Nova::Obj_crd(native_thread().exc_pt_sel, Nova::NUM_INITIAL_PT_LOG2));

	cap_map().remove(native_thread().ec_sel, 1, false);
	cap_map().remove(native_thread().exc_pt_sel, Nova::NUM_INITIAL_PT_LOG2, false);

	/* revoke utcb */
	Nova::Rights rwx(true, true, true);
	addr_t utcb = reinterpret_cast<addr_t>(&_stack->utcb());
	Nova::revoke(Nova::Mem_crd(utcb >> 12, 0, rwx));
}


void Thread::start()
{
	/*
	 * On NOVA, core almost never starts regular threads. This simply creates a
	 * local EC
	 */
	using namespace Nova;

	addr_t sp   = _stack->top();
	Utcb  &utcb = *reinterpret_cast<Utcb *>(&_stack->utcb());

	/* create local EC */
	enum { LOCAL_THREAD = false };
	unsigned const kernel_cpu_id = platform_specific().kernel_cpu_id(_affinity);
	uint8_t res = create_ec(native_thread().ec_sel,
	                        platform_specific().core_pd_sel(), kernel_cpu_id,
	                        (mword_t)&utcb, sp, native_thread().exc_pt_sel, LOCAL_THREAD);
	if (res != NOVA_OK) {
		error("create_ec returned ", res);
		throw Cpu_session::Thread_creation_failed();
	}

	/* default: we don't accept any mappings or translations */
	utcb.crd_rcv = Obj_crd();
	utcb.crd_xlt = Obj_crd();

	if (map_local(platform_specific().core_pd_sel(),
	              *reinterpret_cast<Nova::Utcb *>(Thread::myself()->utcb()),
	              Obj_crd(PT_SEL_PAGE_FAULT, 0),
	              Obj_crd(native_thread().exc_pt_sel + PT_SEL_PAGE_FAULT, 0))) {
		error("could not create page fault portal");
		throw Cpu_session::Thread_creation_failed();
	}

	struct Core_trace_source : public  Trace::Source::Info_accessor,
	                           private Trace::Control,
	                           private Trace::Source
	{
		Thread &thread;

		/**
		 * Trace::Source::Info_accessor interface
		 */
		Info trace_source_info() const override
		{
			uint64_t ec_time = 0;

			uint8_t res = Nova::ec_time(thread.native_thread().ec_sel, ec_time);
			if (res != Nova::NOVA_OK)
				warning("ec_time for core thread failed res=", res);

			return { Session_label("core"), thread.name(),
			         Trace::Execution_time(ec_time, 0), thread._affinity };
		}

		Core_trace_source(Trace::Source_registry &registry, Thread &t)
		:
			Trace::Control(),
			Trace::Source(*this, *this), thread(t)
		{
			registry.insert(this);
		}
	};

	new (platform().core_mem_alloc())
		Core_trace_source(Trace::sources(), *this);
}
