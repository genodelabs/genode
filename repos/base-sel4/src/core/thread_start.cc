/*
 * \brief  Implementation of Thread API interface for core
 * \author Norman Feske
 * \date   2015-05-01
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/thread.h>
#include <base/log.h>
#include <base/sleep.h>

/* base-internal includes */
#include <base/internal/stack.h>

/* core includes */
#include <platform.h>
#include <platform_thread.h>
#include <thread_sel4.h>
#include <trace/source_registry.h>

/* seL4 includes */
#include <sel4/benchmark_utilisation_types.h>

using namespace Genode;


void Thread::_init_platform_thread(size_t, Type type)
{
	addr_t const utcb_virt_addr = (addr_t)&_stack->utcb();

	if (type == MAIN) {
		native_thread().tcb_sel = seL4_CapInitThreadTCB;
		native_thread().lock_sel = INITIAL_SEL_LOCK;
		return;
	}

	Thread_info thread_info;
	thread_info.init(utcb_virt_addr, CONFIG_NUM_PRIORITIES - 1);

	if (!map_local(thread_info.ipc_buffer_phys, utcb_virt_addr, 1)) {
		error(__func__, ": could not map IPC buffer "
		      "phys=",   Hex(thread_info.ipc_buffer_phys), " "
		      "local=%", Hex(utcb_virt_addr));
	}

	native_thread().tcb_sel  = thread_info.tcb_sel.value();
	native_thread().ep_sel   = thread_info.ep_sel.value();
	native_thread().lock_sel = thread_info.lock_sel.value();

	Platform &platform = platform_specific();

	seL4_CNode_CapData guard = seL4_CNode_CapData_new(0, CONFIG_WORD_SIZE - 32);
	seL4_CNode_CapData no_cap_data = { { 0 } };
	int const ret = seL4_TCB_SetSpace(native_thread().tcb_sel, 0,
	                                  platform.top_cnode().sel().value(),
	                                  guard.words[0],
	                                  seL4_CapInitThreadPD, no_cap_data.words[0]);
	ASSERT(ret == seL4_NoError);

	/* mint notification object with badge - used by Genode::Lock */
	Cap_sel unbadged_sel = thread_info.lock_sel;
	Cap_sel lock_sel = platform.core_sel_alloc().alloc();

	platform.core_cnode().mint(platform.core_cnode(), unbadged_sel, lock_sel);

	native_thread().lock_sel = lock_sel.value();
}


void Thread::_deinit_platform_thread()
{
	addr_t const utcb_virt_addr = (addr_t)&_stack->utcb();

	bool ret = unmap_local(utcb_virt_addr, 1);
	ASSERT(ret);

	int res = seL4_CNode_Delete(seL4_CapInitThreadCNode,
	                            native_thread().lock_sel, 32);
	if (res)
		error(__PRETTY_FUNCTION__, ": seL4_CNode_Delete (",
		      Hex(native_thread().lock_sel), ") returned ", res);

	platform_specific().core_sel_alloc().free(Cap_sel(native_thread().lock_sel));
}


void Thread::_thread_start()
{
	Thread::myself()->_thread_bootstrap();
	Thread::myself()->entry();

	sleep_forever();
}


void Thread::start()
{
	start_sel4_thread(Cap_sel(native_thread().tcb_sel), (addr_t)&_thread_start,
	                  (addr_t)stack_top(), _affinity.xpos());

	struct Core_trace_source : public  Trace::Source::Info_accessor,
	                           private Trace::Control,
	                           private Trace::Source
	{
		Thread &_thread;

		/**
		 * Trace::Source::Info_accessor interface
		 */
		Info trace_source_info() const override
		{
			Thread &myself = *Thread::myself();

			seL4_IPCBuffer &ipc_buffer = *reinterpret_cast<seL4_IPCBuffer *>(myself.utcb());
			uint64_t const * const buf =  reinterpret_cast<uint64_t *>(ipc_buffer.msg);

			seL4_BenchmarkGetThreadUtilisation(_thread.native_thread().tcb_sel);
			uint64_t const thread_time = buf[BENCHMARK_TCB_UTILISATION];

			return { Session_label("core"), _thread.name(),
			         Trace::Execution_time(thread_time, 0), _thread._affinity };
		}


		Core_trace_source(Trace::Source_registry &registry, Thread &t)
		:
			Trace::Control(),
			Trace::Source(*this, *this), _thread(t)
		{
			registry.insert(this);
		}
	};

	new (platform().core_mem_alloc())
		Core_trace_source(Trace::sources(), *this);
}


void Thread::cancel_blocking()
{
	warning(__func__, " not implemented");
}


Native_utcb *Thread::utcb()
{
	if (!_stack)
		return nullptr;

	return &_stack->utcb();
}
