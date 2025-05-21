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

using namespace Core;


void Thread::_init_native_thread(Stack &stack, size_t, Type type)
{
	Native_thread &nt = stack.native_thread();

	Utcb_virt const utcb_virt { addr_t(&stack.utcb()) };

	if (type == MAIN) {
		nt.attr.tcb_sel = seL4_CapInitThreadTCB;
		nt.attr.lock_sel = INITIAL_SEL_LOCK;
		return;
	}

	bool const auto_deallocate = false;

	Thread_info thread_info;
	thread_info.init(utcb_virt, CONFIG_NUM_PRIORITIES - 1, auto_deallocate);

	thread_info.ipc_phys.with_result([&](auto &result) {
		if (!map_local(addr_t(result.ptr), utcb_virt.addr, 1)) {
			error(__func__, ": could not map IPC buffer "
			      "phys=", result.ptr, " local=", Hex(utcb_virt.addr));
		}
	}, [](auto) { error(__func__, " IPC buffer error"); });

	nt.attr.tcb_sel  = thread_info.tcb_sel.value();
	nt.attr.ep_sel   = thread_info.ep_sel.value();
	nt.attr.lock_sel = thread_info.lock_sel.value();

	Platform &platform = platform_specific();

	seL4_CNode_CapData guard = seL4_CNode_CapData_new(0, CONFIG_WORD_SIZE - 32);
	seL4_CNode_CapData no_cap_data = { { 0 } };
	int const ret = seL4_TCB_SetSpace(nt.attr.tcb_sel, 0,
	                                  platform.top_cnode().sel().value(),
	                                  guard.words[0],
	                                  seL4_CapInitThreadPD, no_cap_data.words[0]);
	ASSERT(ret == seL4_NoError);

	/* mint notification object with badge - used by Genode::Lock */
	Cap_sel unbadged_sel = thread_info.lock_sel;
	Cap_sel lock_sel = platform.core_sel_alloc().alloc();

	platform.core_cnode().mint(platform.core_cnode(), unbadged_sel, lock_sel);

	nt.attr.lock_sel = lock_sel.value();
}


void Thread::_deinit_native_thread(Stack &stack)
{
	addr_t const utcb_virt_addr = addr_t(&stack.utcb());

	bool ret = unmap_local(utcb_virt_addr, 1);
	ASSERT(ret);

	int res = seL4_CNode_Delete(seL4_CapInitThreadCNode,
	                            stack.native_thread().attr.lock_sel, 32);
	if (res)
		error(__PRETTY_FUNCTION__, ": seL4_CNode_Delete (",
		      Hex(stack.native_thread().attr.lock_sel), ") returned ", res);

	platform_specific().core_sel_alloc().free(Cap_sel(stack.native_thread().attr.lock_sel));
}


void Thread::_thread_start()
{
	Thread::myself()->_thread_bootstrap();
	Thread::myself()->entry();

	sleep_forever();
}


namespace {

	struct Core_trace_source : public  Core::Trace::Source::Info_accessor,
	                           private Core::Trace::Control,
	                           private Core::Trace::Source
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

			_thread.with_native_thread([&] (Native_thread &nt) {
				seL4_BenchmarkGetThreadUtilisation(nt.attr.tcb_sel); });

			uint64_t const thread_time = buf[BENCHMARK_TCB_UTILISATION];

			return { Session_label("core"), _thread.name,
			         Genode::Trace::Execution_time(thread_time, 0), _thread.affinity() };
		}

		Core_trace_source(Core::Trace::Source_registry &registry, Thread &t)
		:
			Core::Trace::Control(),
			Core::Trace::Source(*this, *this), _thread(t)
		{
			registry.insert(this);
		}
	};
}


Thread::Start_result Thread::start()
{
	return _stack.convert<Start_result>([&] (Stack &stack) {

		/* write ipcbuffer address to utcb*/
		utcb()->ipcbuffer(Native_utcb::Virt { addr_t(utcb()) });

		start_sel4_thread(Cap_sel(stack.native_thread().attr.tcb_sel),
		                  addr_t(&_thread_start), stack.top(),
		                  _affinity.xpos(), addr_t(utcb()));
		try {
			new (platform().core_mem_alloc())
				Core_trace_source(Core::Trace::sources(), *this);
		} catch (...) { }

		return Start_result::OK;

	}, [&] (Stack_error) { return Start_result::DENIED; });
}


Native_utcb *Thread::utcb()
{
	return _stack.convert<Native_utcb *>(
		[] (Stack &stack) { return &stack.utcb(); },
		[] (Stack_error)  { return nullptr; });
}
