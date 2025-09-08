/*
 * \brief  Implementation of Thread API interface for core
 * \author Stefan Kalkowski
 * \author Martin Stein
 * \date   2014-02-27
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/thread.h>
#include <base/sleep.h>
#include <base/env.h>

/* base-internal stack */
#include <base/internal/stack.h>
#include <base/internal/native_utcb.h>

/* core includes */
#include <map_local.h>
#include <platform.h>
#include <platform_thread.h>
#include <trace/source_registry.h>

using namespace Core;

namespace Hw { extern Untyped_capability _main_thread_cap; }


namespace {

	struct Trace_source : public  Core::Trace::Source::Info_accessor,
	                      private Core::Trace::Control,
	                      private Core::Trace::Source
	{
		Genode::Thread &thread;

		/**
		 * Trace::Source::Info_accessor interface
		 */
		Info trace_source_info() const override
		{
			Genode::Trace::Execution_time execution_time { 0, 0 };

			thread.with_native_thread([&] (Native_thread &nt) {
				if (nt.platform_thread)
					execution_time = nt.platform_thread->execution_time(); });

			return { Session_label("core"), thread.name,
			         execution_time, thread.affinity() };
		}

		Trace_source(Core::Trace::Source_registry &registry, Genode::Thread &thread)
		:
			Core::Trace::Control(),
			Core::Trace::Source(*this, *this),
			thread(thread)
		{
			registry.insert(this);
		}
	};
}


Thread::Start_result Thread::start()
{
	return _stack.convert<Start_result>([&] (Stack &stack) {

		Native_thread &nt = stack.native_thread();

		/* start thread with stack pointer at the top of stack */
		if (nt.platform_thread)
			nt.platform_thread->start((void *)&_thread_start, (void *)stack.top());

		if (_thread_cap.failed())
			return Start_result::DENIED;;

		/* create trace sources for core threads */
		try {
			new (platform().core_mem_alloc()) Trace_source(Core::Trace::sources(), *this);
		} catch (...) { }

		return Start_result::OK;

	}, [&] (Stack_error) { return Start_result::DENIED; });
}


void Thread::_deinit_native_thread(Stack &stack)
{
	destroy(platform().core_mem_alloc(), stack.native_thread().platform_thread);
}


void Thread::_init_native_thread(Stack &stack, Type type)
{
	if (type == NORMAL) {
		_stack.with_result([&] (Stack &stack) {
			stack.native_thread().platform_thread = new (platform().core_mem_alloc())
				Platform_thread(name, stack.utcb(), _affinity);
		}, [&] (Stack_error) { });
		return;
	}

	/* remap initial main-thread UTCB according to stack-area spec */
	map_local(Platform::core_main_thread_phys_utcb(),
	          (addr_t)&stack.utcb(),
	          max(sizeof(Native_utcb) / get_page_size(), (size_t)1));

	/* adjust initial object state in case of a main thread */
	stack.native_thread().cap = Hw::_main_thread_cap;
}
