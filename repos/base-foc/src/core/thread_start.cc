/*
 * \brief  Fiasco.OC-specific implementation of core's startup Thread API.
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \date   2006-05-03
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/thread.h>
#include <base/sleep.h>
#include <trace/source_registry.h>

/* base-internal includes */
#include <base/internal/stack.h>

/* core includes */
#include <platform.h>

/* Fiasco.OC includes */
#include <foc/syscall.h>

using namespace Core;


void Thread::_deinit_native_thread(Stack &) { }


void Thread::_init_native_thread(Stack &, size_t, Type) { }


namespace {

	struct Core_trace_source : public  Core::Trace::Source::Info_accessor,
	                           private Core::Trace::Control,
	                           private Core::Trace::Source
	{
		Thread &thread;
		Platform_thread &platform_thread;

		/**
		 * Trace::Source::Info_accessor interface
		 */
		Info trace_source_info() const override
		{
			uint64_t const sc_time = 0;

			using namespace Foc;
			l4_kernel_clock_t ec_time = 0;

			/*
			 * The 'l4_thread_stats_time' syscall does not always return if
			 * the thread is on remote CPU. Disable the feature to keep core
			 * safe (see issue #4357).
			 */
			if (0) {
				addr_t const kcap = (addr_t) platform_thread.pager_object_badge();
				l4_msgtag_t res = l4_thread_stats_time(kcap, &ec_time);
				if (l4_error(res))
					Genode::error("cpu time for ", thread.name,
					              " is not available ", l4_error(res));
			}

			return { Session_label("core"), thread.name,
			         Genode::Trace::Execution_time(ec_time, sc_time, 10000,
			                                       platform_thread.prio()),
			         thread.affinity() };
		}

		Core_trace_source(Core::Trace::Source_registry &registry, Thread &t,
		                  Platform_thread &pt)
		:
			Core::Trace::Control(),
			Core::Trace::Source(*this, *this), thread(t), platform_thread(pt)
		{
			registry.insert(this);
		}
	};
}


Thread::Start_result Thread::start()
{
	using namespace Foc;

	try {
		/* create and start platform thread */
		Platform_thread &pt = *new (platform().core_mem_alloc())
			Platform_thread(name.string());

		platform_specific().core_pd().bind_thread(pt);

		l4_utcb_t * const foc_utcb = (l4_utcb_t *)(pt.utcb());

		with_native_thread([&] (Native_thread &nt) {
			nt.kcap = pt.gate().remote; });

		utcb()->foc_utcb = foc_utcb;

		_thread_cap =
			reinterpret_cap_cast<Cpu_thread>(Native_capability(pt.thread().local));

		pt.pager(platform_specific().core_pager());

		l4_utcb_tcr_u(foc_utcb)->user[UTCB_TCR_BADGE] = (unsigned long) pt.gate().local.data();
		l4_utcb_tcr_u(foc_utcb)->user[UTCB_TCR_THREAD_OBJ] = (addr_t)this;

		return _stack.convert<Start_result>(
			[&] (Stack &stack) {
				pt.start((void *)_thread_start, (void *)stack.top());

				try {
					new (platform().core_mem_alloc())
						Core_trace_source(Core::Trace::sources(), *this, pt);
				}
				catch (...) { }

				return Start_result::OK;
			},
			[&] (Stack_error) { return Start_result::DENIED; });
	}
	catch (...) { }

	return Start_result::DENIED;
}
