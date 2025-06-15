/*
 * \brief  Implementation of Thread API interface for core
 * \author Norman Feske
 * \author Alexander Boettcher
 * \date   2015-05-01
 */

/*
 * Copyright (C) 2015-2025 Genode Labs GmbH
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


enum { CORE_MAX_THREADS = stack_area_virtual_size() /
                          stack_virtual_size() };


static auto & _raw_thread_info_storage()
{
	/* not instantiated within with_thread_info() due to template */
	static Thread_info thread_infos[CORE_MAX_THREADS];
	return thread_infos;
}


static bool with_thread_info(Stack &stack, auto const &fn)
{
	auto &thread_infos = _raw_thread_info_storage();

	if (stack.top() < stack_area_virtual_base())
		return false;

	auto const id = (stack.top() - stack_area_virtual_base()) / stack_virtual_size();

	if (id >= CORE_MAX_THREADS)
		return false;

	return fn(thread_infos[id]);
}


void Thread::_init_native_thread(Stack &stack, size_t, Type type)
{
	Native_thread &nt = stack.native_thread();

	Utcb_virt const utcb_virt { addr_t(&stack.utcb()) };

	if (type == MAIN) {
		nt.attr.tcb_sel = seL4_CapInitThreadTCB;
		nt.attr.lock_sel = INITIAL_SEL_LOCK;
		return;
	}

	with_thread_info(stack, [&](auto &thread_info){

		thread_info.init(utcb_virt, CONFIG_NUM_PRIORITIES - 1);

		bool ok = thread_info.ipc_phys.template convert<bool>([&](auto &result) {
			if (!map_local(addr_t(result.ptr), utcb_virt.addr, 1)) {
				error(__func__, ": could not map IPC buffer "
				      "phys=", result.ptr, " local=", Hex(utcb_virt.addr));
				return false;
			}
			return true;
		}, [](auto) {
			error(__func__, " IPC buffer error");
			return false;
		});

		if (!ok)
			return ok;

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
		if (ret != seL4_NoError) {
			error("seL4_TCB_SetSpace failed");
			return false;
		}

		/* mint notification object with badge - used by Genode::Lock */
		Cap_sel const unbadged_sel = thread_info.lock_sel;

		return platform.core_sel_alloc().alloc().convert<bool>([&](auto sel) {
			auto lock_sel = Cap_sel(unsigned(sel));

			if (!platform.core_cnode().mint(platform.core_cnode(), unbadged_sel,
			                                lock_sel)) {
				warning("core thread: mint failed");
				return false;
			}

			nt.attr.lock_sel = lock_sel.value();

			/* remember for destruction of thread, e.g. IRQ thread */
			thread_info.lock_sel          = lock_sel;
			thread_info.lock_sel_unminted = unbadged_sel;

			return true;
		}, [](auto) {
			warning("core thread: selector allocation failed");
			return false;
		});
	});
}


void Thread::_deinit_native_thread(Stack &stack)
{
	with_thread_info(stack, [&](auto &thread_info){
		addr_t const utcb_virt_addr = addr_t(&stack.utcb());

		if (!unmap_local(utcb_virt_addr, 1))
			error("could not unmap IPC buffer");

		thread_info.destruct();
		/* trigger auto deallocate of phys* and re-init to default values */
		thread_info = { };

		return true;
	});
}


void Thread::_thread_start()
{
	Thread::myself()->_thread_bootstrap();
	Thread::myself()->entry();
	Thread::myself()->_join.wakeup();

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

		if (!start_sel4_thread(Cap_sel(stack.native_thread().attr.tcb_sel),
		                       addr_t(&_thread_start), stack.top(),
		                       _affinity.xpos(), addr_t(utcb()))) {
			return Start_result::DENIED;
		}

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
