/*
 * \brief  Core implementation of the CPU session/thread interfaces
 * \author Christian Helmuth
 * \date   2006-07-17
 *
 * FIXME arg_string and quota missing
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <util/arg_string.h>

/* core includes */
#include <cpu_session_component.h>
#include <rm_session_component.h>
#include <pd_session_component.h>
#include <platform_generic.h>

using namespace Core;


Cpu_session::Create_thread_result
Cpu_session_component::create_thread(Capability<Pd_session> pd_cap,
                                     Name const &name, Affinity::Location affinity,
                                     addr_t utcb)
{
	if (!try_withdraw(Ram_quota{_utcb_quota_size()}))
		return Create_thread_error::OUT_OF_RAM;

	Mutex::Guard thread_list_lock_guard(_thread_list_lock);

	Create_thread_result result = Create_thread_error::DENIED;

	_thread_ep.apply(pd_cap, [&] (Pd_session_component *pd) {

		if (!pd) {
			error("create_thread: invalid PD argument");
			return;
		}

		Mutex::Guard slab_lock_guard(_thread_alloc_lock);

		pd->with_threads([&] (Pd_session_component::Threads &pd_threads) {
			pd->with_platform_pd([&] (Platform_pd &platform_pd) {

				_thread_alloc.create(cap(), *this, _thread_ep, _local_rm,
				                     _pager_ep, *pd, _ram_alloc, platform_pd,
				                     pd_threads, _trace_control_area,
				                     _trace_sources,
				                     _thread_affinity(affinity), _label, name,
				                     _priority, utcb).with_result(

					[&] (Thread_alloc::Allocation &thread) {
						thread.obj.constructed.with_result(
							[&] (Ok) {
								thread.obj.session_exception_sigh(_exception_sigh);
								_thread_list.insert(&thread.obj);
								thread.deallocate = false;
								result = thread.obj.cap();
							},
							[&] (Alloc_error e) { result = e; });
					},
					[&] (Alloc_error e) { result = e; });
			});
		});
	});

	if (result.failed())
		replenish(Ram_quota{_utcb_quota_size()});

	return result;
}


Affinity::Location Cpu_session_component::_thread_affinity(Affinity::Location location) const
{
	/* convert session-local location to physical location */
	int const x1 = location.xpos() + _location.xpos(),
	          y1 = location.ypos() + _location.ypos(),
	          x2 = location.xpos() +  location.width(),
	          y2 = location.ypos() +  location.height();

	int const clipped_x1 = max(_location.xpos(), x1),
	          clipped_y1 = max(_location.ypos(), y1),
	          clipped_x2 = max(_location.xpos() + (int)_location.width()  - 1, x2),
	          clipped_y2 = max(_location.ypos() + (int)_location.height() - 1, y2);

	return Affinity::Location(clipped_x1, clipped_y1,
	                          clipped_x2 - clipped_x1 + 1,
	                          clipped_y2 - clipped_y1 + 1);
}


void Cpu_session_component::_unsynchronized_kill_thread(Thread_capability thread_cap)
{
	Cpu_thread_component *thread_ptr = nullptr;
	_thread_ep.apply(thread_cap, [&] (Cpu_thread_component *t) { thread_ptr = t; });

	if (!thread_ptr) return;

	_thread_list.remove(thread_ptr);

	{
		Mutex::Guard lock_guard(_thread_alloc_lock);
		_thread_alloc.destroy(*thread_ptr);
	}

	replenish(Ram_quota{_utcb_quota_size()});
}


void Cpu_session_component::kill_thread(Thread_capability thread_cap)
{
	if (!thread_cap.valid())
		return;

	Mutex::Guard lock_guard(_thread_list_lock);

	/* check that cap belongs to this session */
	for (Cpu_thread_component *t = _thread_list.first(); t; t = t->next()) {
		if (t->cap() == thread_cap) {
			_unsynchronized_kill_thread(thread_cap);
			break;
		}
	}
}


void Cpu_session_component::exception_sigh(Signal_context_capability sigh)
{
	_exception_sigh = sigh;

	Mutex::Guard lock_guard(_thread_list_lock);

	for (Cpu_thread_component *t = _thread_list.first(); t; t = t->next())
		t->session_exception_sigh(_exception_sigh);
}


Affinity::Space Cpu_session_component::affinity_space() const
{
	/*
	 * Return affinity subspace as constrained by the CPU session
	 * affinity.
	 */
	return Affinity::Space(_location.width(), _location.height());
}


Dataspace_capability Cpu_session_component::trace_control()
{
	return _trace_control_area.dataspace();
}


Cpu_session_component::Cpu_session_component(Rpc_entrypoint         &session_ep,
                                             Resources        const &resources,
                                             Label            const &label,
                                             Diag             const &diag,
                                             Ram_allocator          &ram_alloc,
                                             Local_rm               &local_rm,
                                             Rpc_entrypoint         &thread_ep,
                                             Pager_entrypoint       &pager_ep,
                                             Trace::Source_registry &trace_sources,
                                             char             const *args,
                                             Affinity         const &affinity)
:
	Session_object(session_ep, resources, label, diag),
	_session_ep(session_ep), _thread_ep(thread_ep), _pager_ep(pager_ep),
	_local_rm(local_rm),
	_ram_alloc(ram_alloc, _ram_quota_guard(), _cap_quota_guard()),
	_md_alloc(_ram_alloc, local_rm),
	_thread_slab(_md_alloc), _priority(0),

	/* map affinity to a location within the physical affinity space */
	_location(affinity.scale_to(platform().affinity_space())),

	_trace_sources(trace_sources),
	_trace_control_area(_ram_alloc, local_rm),
	_native_cpu(*this, args)
{
	Arg a = Arg_string::find_arg(args, "priority");
	if (a.valid()) {
		_priority = (unsigned)a.ulong_value(0);

		/* clamp priority value to valid range */
		_priority = min((unsigned)(PRIORITY_LIMIT - 1), _priority);
	}
}


Cpu_session_component::~Cpu_session_component()
{
	_deinit_threads();
}


void Cpu_session_component::_deinit_threads()
{
	Mutex::Guard lock_guard(_thread_list_lock);

	/*
	 * We have to keep the '_thread_list_lock' during the whole destructor to
	 * prevent races with incoming calls of the 'create_thread' function,
	 * adding new threads while we are destroying them.
	 */

	for (Cpu_thread_component *thread; (thread = _thread_list.first()); )
		_unsynchronized_kill_thread(thread->cap());
}


/****************************
 ** Trace::Source_registry **
 ****************************/

Core::Trace::Source::Id Core::Trace::Source::_alloc_unique_id()
{
	static Mutex lock;
	static unsigned cnt;
	Mutex::Guard guard(lock);
	return { cnt++ };
}

