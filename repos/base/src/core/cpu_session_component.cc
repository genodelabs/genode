/*
 * \brief  Core implementation of the CPU session/thread interfaces
 * \author Christian Helmuth
 * \date   2006-07-17
 *
 * FIXME arg_string and quota missing
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <util/arg_string.h>

/* core includes */
#include <cpu_session_component.h>
#include <rm_session_component.h>
#include <platform_generic.h>

using namespace Genode;

static constexpr bool verbose = false;

void Cpu_thread_component::update_exception_sigh()
{
	if (platform_thread()->pager())
		platform_thread()->pager()->exception_handler(_sigh);
};


Thread_capability Cpu_session_component::create_thread(size_t weight,
                                                       Name const &name,
                                                       addr_t utcb)
{
	unsigned trace_control_index = 0;
	if (!_trace_control_area.alloc(trace_control_index))
		throw Out_of_metadata();

	Trace::Control * const trace_control =
		_trace_control_area.at(trace_control_index);

	Trace::Thread_name thread_name(name.string());

	Cpu_thread_component *thread = 0;

	if (weight == 0) {
		PWRN("Thread %s: Bad weight 0, using %i instead.",
		     name.string(), DEFAULT_WEIGHT);
		weight = DEFAULT_WEIGHT;
	}
	if (weight > QUOTA_LIMIT) {
		PWRN("Thread %s: Oversized weight %zu, using %i instead.",
		     name.string(), weight, QUOTA_LIMIT);
		weight = QUOTA_LIMIT;
	}
	Lock::Guard thread_list_lock_guard(_thread_list_lock);
	_incr_weight(weight);

	try {
		Lock::Guard slab_lock_guard(_thread_alloc_lock);
		thread = new(&_thread_alloc)
			Cpu_thread_component(
				weight, _weight_to_quota(weight), _label, thread_name,
				_priority, utcb, _default_exception_handler,
				trace_control_index, *trace_control);

		/* set default affinity defined by CPU session */
		thread->platform_thread()->affinity(_location);
	} catch (Allocator::Out_of_memory) {
		throw Out_of_metadata();
	}

	_thread_list.insert(thread);

	_trace_sources.insert(thread->trace_source());

	return _thread_ep->manage(thread);
}


void Cpu_session_component::_unsynchronized_kill_thread(Cpu_thread_component *thread)
{
	_thread_ep->dissolve(thread);
	_thread_list.remove(thread);

	_trace_sources.remove(thread->trace_source());

	unsigned const trace_control_index = thread->trace_control_index();

	_decr_weight(thread->weight());

	Lock::Guard lock_guard(_thread_alloc_lock);
	destroy(&_thread_alloc, thread);

	_trace_control_area.free(trace_control_index);
}


void Cpu_session_component::kill_thread(Thread_capability thread_cap)
{
	Cpu_thread_component * thread =
		dynamic_cast<Cpu_thread_component *>(_thread_ep->lookup_and_lock(thread_cap));
	if (!thread) return;

	Lock::Guard lock_guard(_thread_list_lock);
	_unsynchronized_kill_thread(thread);
}


int Cpu_session_component::set_pager(Thread_capability thread_cap,
                                     Pager_capability  pager_cap)
{
	Object_pool<Cpu_thread_component>::Guard thread(_thread_ep->lookup_and_lock(thread_cap));
	if (!thread) return -1;

	Object_pool<Pager_object>::Guard p(_pager_ep->lookup_and_lock(pager_cap));
	if (!p) return -2;

	thread->platform_thread()->pager(p);

	p->thread_cap(thread->cap());
   
	return 0;
}


int Cpu_session_component::start(Thread_capability thread_cap,
                                 addr_t ip, addr_t sp)
{
	Object_pool<Cpu_thread_component>::Guard thread(_thread_ep->lookup_and_lock(thread_cap));
	if (!thread) return -1;

	/*
	 * If an exception handler was installed prior to the call of 'set_pager',
	 * we need to update the pager object with the current exception handler.
	 */
	thread->update_exception_sigh();

	return thread->platform_thread()->start((void *)ip, (void *)sp);
}


void Cpu_session_component::pause(Thread_capability thread_cap)
{
	Object_pool<Cpu_thread_component>::Guard thread(_thread_ep->lookup_and_lock(thread_cap));
	if (!thread) return;

	thread->platform_thread()->pause();
}


void Cpu_session_component::resume(Thread_capability thread_cap)
{
	Object_pool<Cpu_thread_component>::Guard thread(_thread_ep->lookup_and_lock(thread_cap));
	if (!thread) return;

	thread->platform_thread()->resume();
}


void Cpu_session_component::cancel_blocking(Thread_capability thread_cap)
{
	Object_pool<Cpu_thread_component>::Guard thread(_thread_ep->lookup_and_lock(thread_cap));
	if (!thread) return;

	thread->platform_thread()->cancel_blocking();
}


Thread_state Cpu_session_component::state(Thread_capability thread_cap)
{
	Object_pool<Cpu_thread_component>::Guard thread(_thread_ep->lookup_and_lock(thread_cap));
	if (!thread) throw State_access_failed();

	return thread->platform_thread()->state();
}


void Cpu_session_component::state(Thread_capability thread_cap,
                                  Thread_state const &state)
{
	Object_pool<Cpu_thread_component>::Guard thread(_thread_ep->lookup_and_lock(thread_cap));
	if (!thread) throw State_access_failed();

	thread->platform_thread()->state(state);
}


void
Cpu_session_component::exception_handler(Thread_capability         thread_cap,
                                         Signal_context_capability sigh_cap)
{
	/*
	 * By specifying an invalid thread capability, the caller sets the default
	 * exception handler for the CPU session.
	 */
	if (!thread_cap.valid()) {
		_default_exception_handler = sigh_cap;
		return;
	}

	/*
	 * If an invalid signal handler is specified for a valid thread, we revert
	 * the signal handler to the CPU session's default signal handler.
	 */
	if (!sigh_cap.valid()) {
		sigh_cap = _default_exception_handler;
	}

	Object_pool<Cpu_thread_component>::Guard thread(_thread_ep->lookup_and_lock(thread_cap));
	if (!thread) return;

	thread->sigh(sigh_cap);
}


Affinity::Space Cpu_session_component::affinity_space() const
{
	/*
	 * Return affinity subspace as constrained by the CPU session
	 * affinity.
	 */
	return Affinity::Space(_location.width(), _location.height());
}


void Cpu_session_component::affinity(Thread_capability  thread_cap,
                                     Affinity::Location location)
{
	Object_pool<Cpu_thread_component>::Guard thread(_thread_ep->lookup_and_lock(thread_cap));
	if (!thread) return;

	/* convert session-local location to physical location */
	int const x1 = location.xpos() + _location.xpos(),
	          y1 = location.ypos() + _location.ypos(),
	          x2 = location.xpos() +  location.width(),
	          y2 = location.ypos() +  location.height();

	int const clipped_x1 = max(_location.xpos(), x1),
	          clipped_y1 = max(_location.ypos(), y1),
	          clipped_x2 = max(_location.xpos() + (int)_location.width()  - 1, x2),
	          clipped_y2 = max(_location.ypos() + (int)_location.height() - 1, y2);

	thread->platform_thread()->affinity(Affinity::Location(clipped_x1, clipped_y1,
	                                                       clipped_x2 - clipped_x1 + 1,
	                                                       clipped_y2 - clipped_y1 + 1));
}


Dataspace_capability Cpu_session_component::trace_control()
{
	return _trace_control_area.dataspace();
}


unsigned Cpu_session_component::trace_control_index(Thread_capability thread_cap)
{
	Object_pool<Cpu_thread_component>::Guard thread(_thread_ep->lookup_and_lock(thread_cap));
	if (!thread) return 0;

	return thread->trace_control_index();
}


Dataspace_capability Cpu_session_component::trace_buffer(Thread_capability thread_cap)
{
	Object_pool<Cpu_thread_component>::Guard thread(_thread_ep->lookup_and_lock(thread_cap));
	if (!thread) return Dataspace_capability();

	return thread->trace_source()->buffer();
}


Dataspace_capability Cpu_session_component::trace_policy(Thread_capability thread_cap)
{
	Object_pool<Cpu_thread_component>::Guard thread(_thread_ep->lookup_and_lock(thread_cap));
	if (!thread) return Dataspace_capability();

	return thread->trace_source()->policy();
}


static size_t remaining_session_ram_quota(char const *args)
{
	/*
	 * We don't need to consider an underflow here because
	 * 'Cpu_root::_create_session' already checks for the condition.
	 */
	return Arg_string::find_arg(args, "ram_quota").ulong_value(0)
	     - Trace::Control_area::SIZE;
}


void Cpu_session_component::_transfer_quota(Cpu_session_component * const dst,
                                            size_t const quota)
{
	if (!quota) { return; }
	_decr_quota(quota);
	dst->_incr_quota(quota);
}


int Cpu_session_component::transfer_quota(Cpu_session_capability dst_cap,
                                          size_t amount)
{
	/* lookup targeted CPU session */
	Object_pool<Cpu_session_component>::Guard
		dst(_session_ep->lookup_and_lock(dst_cap));
	if (!dst) {
		PWRN("Transfer CPU quota, %s, targeted session not found",
		     _label.string());
		return -1;
	}
	/* check reference relationship */
	if (dst->_ref != this && dst != _ref) {
		PWRN("Transfer CPU quota, %s -> %s, no reference relation",
		     _label.string(), dst->_label.string());
		return -2;
	}
	/* check quota availability */
	size_t const quota = quota_lim_downscale(_quota, amount);
	if (quota > _quota) {
		PWRN("Transfer CPU quota, %s -> %s, insufficient quota %zu, need %zu",
		     _label.string(), dst->_label.string(), _quota, quota);
		return -3;
	}
	/* transfer quota */
	_transfer_quota(dst, quota);
	return 0;
}


int Cpu_session_component::ref_account(Cpu_session_capability ref_cap)
{
	/*
	 * Ensure that the ref account is set only once
	 *
	 * FIXME Add check for cycles along the tree of reference accounts
	 */
	if (_ref) {
		PWRN("Set ref account, %s, set already",
		     _label.string());
		return -2; }

	/* lookup and check targeted CPU-session */
	Object_pool<Cpu_session_component>::Guard
		ref(_session_ep->lookup_and_lock(ref_cap));
	if (!ref) {
		PWRN("Set ref account, %s, targeted session not found",
		     _label.string());
		return -1;
	}
	if (ref == this) {
		PWRN("Set ref account, %s, self reference not allowed",
		     _label.string());
		return -3;
	}
	/* establish ref-account relation from targeted CPU-session to us */
	_ref = ref;
	_ref->_insert_ref_member(this);
	return 0;
}


Cpu_session_component::Cpu_session_component(Rpc_entrypoint         *session_ep,
                                             Rpc_entrypoint         *thread_ep,
                                             Pager_entrypoint       *pager_ep,
                                             Allocator              *md_alloc,
                                             Trace::Source_registry &trace_sources,
                                             char             const *args,
                                             Affinity         const &affinity,
                                             size_t           const  quota)
:
	_session_ep(session_ep),
	_thread_ep(thread_ep), _pager_ep(pager_ep),
	_md_alloc(md_alloc, remaining_session_ram_quota(args)),
	_thread_alloc(&_md_alloc), _priority(0),

	/* map affinity to a location within the physical affinity space */
	_location(affinity.scale_to(platform()->affinity_space())),

	_trace_sources(trace_sources), _quota(quota), _ref(0)
{
	/* remember session label */
	char buf[Session_label::size()];
	Arg_string::find_arg(args, "label").string(buf, sizeof(buf), "");
	_label = Session_label(buf);

	Arg a = Arg_string::find_arg(args, "priority");
	if (a.valid()) {
		_priority = a.ulong_value(0);

		/* clamp priority value to valid range */
		_priority = min((unsigned)PRIORITY_LIMIT - 1, _priority);
	}
}


Cpu_session_component::~Cpu_session_component()
{
	_deinit_threads();
	_deinit_ref_account();
}


void Cpu_session_component::_deinit_ref_account()
{
	/* without a ref-account, nothing has do be done */
	if (!_ref) { return; }

	/* give back our remaining quota to our ref account */
	_transfer_quota(_ref, _quota);

	/* remove ref-account relation between us and our ref-account */
	Cpu_session_component * const orig_ref = _ref;
	_ref->_remove_ref_member(this);

	/* redirect ref-account relation of ref members to our prior ref account */
	Lock::Guard lock_guard(_ref_members_lock);
	for (Cpu_session_component * s; (s = _ref_members.first()); ) {
		_unsync_remove_ref_member(s);
		orig_ref->_insert_ref_member(s);
	}
}


void Cpu_session_component::_deinit_threads()
{
	Lock::Guard lock_guard(_thread_list_lock);

	/*
	 * We have to keep the '_thread_list_lock' during the whole destructor to
	 * prevent races with incoming calls of the 'create_thread' function,
	 * adding new threads while we are destroying them.
	 */

	for (Cpu_thread_component *thread; (thread = _thread_list.first()); )
		_unsynchronized_kill_thread(thread);
}


void Cpu_session_component::
_update_thread_quota(Cpu_thread_component * const thread) const
{
	thread->platform_thread()->quota(_weight_to_quota(thread->weight()));
}


void Cpu_session_component::_incr_weight(size_t const weight)
{
	_weight += weight;
	if (_quota) { _update_each_thread_quota(); }
}


void Cpu_session_component::_decr_weight(size_t const weight)
{
	_weight -= weight;
	if (_quota) { _update_each_thread_quota(); }
}


void Cpu_session_component::_decr_quota(size_t const quota)
{
	Lock::Guard lock_guard(_thread_list_lock);
	_quota -= quota;
	_update_each_thread_quota();
}


void Cpu_session_component::_incr_quota(size_t const quota)
{
	Lock::Guard lock_guard(_thread_list_lock);
	_quota += quota;
	_update_each_thread_quota();
}


void Cpu_session_component::_update_each_thread_quota()
{
	Cpu_thread_component * thread = _thread_list.first();
	for (; thread; thread = thread->next()) { _update_thread_quota(thread); }
}


size_t
Cpu_session_component::_weight_to_quota(size_t const weight) const {
	return (weight * _quota) / _weight; }


/****************************
 ** Trace::Source_registry **
 ****************************/

unsigned Trace::Source::_alloc_unique_id()
{
	static Lock lock;
	static unsigned cnt;
	Lock::Guard guard(lock);
	return cnt++;
}

