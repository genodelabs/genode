/*
 * \brief  Kernel back-end for execution contexts in userland
 * \author Martin Stein
 * \author Stefan Kalkowski
 * \date   2013-09-15
 */

/*
 * Copyright (C) 2013-2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/thread_state.h>
#include <cpu_session/cpu_session.h>

/* base-internal includes */
#include <base/internal/crt0.h>

/* core includes */
#include <hw/assert.h>
#include <hw/memory_map.h>
#include <kernel/cpu.h>
#include <kernel/thread.h>
#include <kernel/irq.h>
#include <kernel/log.h>
#include <map_local.h>
#include <platform.h>

extern "C" void _core_start(void);

using namespace Kernel;


Thread::Ipc_alloc_result Thread::_ipc_alloc_recv_caps(unsigned cap_count)
{
	using Result      = Ipc_alloc_result;
	using Alloc_error = Genode::Alloc_error;

	for (unsigned i = 0; i < cap_count; i++) {
		if (_obj_id_ref_ptr[i] != nullptr)
			continue;

		size_t size = sizeof(Object_identity_reference);
		Result const result = _pd.cap_slab().try_alloc(size).convert<Result>(

			[&] (Genode::Memory::Allocation &a) {
				_obj_id_ref_ptr[i] = a.ptr;
				a.deallocate = false;
				return Result::OK;
			},
			[&] (Alloc_error e) {

				/*
				 * Conditions other than DENIED cannot happen because the slab
				 * does not try to grow automatically. It is explicitely
				 * expanded by the client as response to the EXHAUSTED return
				 * value.
				 */
				if (e != Alloc_error::DENIED)
					Genode::raw("unexpected recv_caps allocation failure");

				return Result::EXHAUSTED;
			}
		);
		if (result == Result::EXHAUSTED)
			return result;
	}
	_ipc_rcv_caps = cap_count;
	return Result::OK;
}


void Thread::_ipc_free_recv_caps()
{
	for (unsigned i = 0; i < _ipc_rcv_caps; i++) {
		if (_obj_id_ref_ptr[i]) {
			_pd.cap_slab().free(_obj_id_ref_ptr[i],
			                     sizeof(Object_identity_reference));
		}
	}
	_ipc_rcv_caps = 0;
}


Thread::Ipc_alloc_result Thread::_ipc_init(Genode::Native_utcb &utcb, Thread &starter)
{
	_utcb = &utcb;

	switch (_ipc_alloc_recv_caps((unsigned)(starter._utcb->cap_cnt()))) {

	case Ipc_alloc_result::OK:
		ipc_copy_msg(starter);
		break;

	case Ipc_alloc_result::EXHAUSTED:
		return Ipc_alloc_result::EXHAUSTED;
	}
	return Ipc_alloc_result::OK;
}


void Thread::_save(Genode::Cpu_state &state)
{
	if (_type == IDLE)
		return;

	Genode::memcpy(&*regs, &state, sizeof(Board::Cpu::Context));
}


void Thread::ipc_copy_msg(Thread &sender)
{
	using namespace Genode;

	/* copy payload and set destination capability id */
	*_utcb = *sender._utcb;
	_utcb->destination(sender._ipc_capid);

	/* translate capabilities */
	for (unsigned i = 0; i < _ipc_rcv_caps; i++) {

		capid_t id = sender._utcb->cap_get(i);

		/* if there is no capability to send, nothing to do */
		if (i >= sender._utcb->cap_cnt()) { continue; }

		capid_t to_add = cap_id_invalid();

		/* lookup the capability id within the caller's cap space */
		sender._pd.cap_tree().with(id,
			[&] (auto &oir) {

				/* lookup the capability id within the callee's cap space */
				oir.with_in_pd(_pd,
					[&] (auto &dst_oir) {
						dst_oir.add_to_utcb();
						to_add = dst_oir.capid();
					},
					[&] () {
						if (&_pd != &_core_pd) {
							oir.factory(_obj_id_ref_ptr[i], _pd,
								[&] (auto &new_oir) {
								_obj_id_ref_ptr[i] = nullptr;
								new_oir.add_to_utcb();
								to_add = new_oir.capid();
							});
						}
					});
			},
			[&] () { /* no cap in caller cap space, do nothing */});

		_utcb->cap_add(to_add);
	}
}


Thread::
Tlb_invalidation::Tlb_invalidation(Inter_processor_work_list &global_work_list,
                                   Thread                    &caller,
                                   Pd                        &pd,
                                   addr_t                     addr,
                                   size_t                     size,
                                   unsigned                   cnt)
:
	global_work_list { global_work_list },
	caller           { caller },
	pd               { pd },
	addr             { addr },
	size             { size },
	cnt              { cnt }
{
	global_work_list.insert(&_le);
	caller._become_inactive(AWAITS_RESTART);
}


template <typename Obj>
Thread::Destroy<Obj>::Destroy(Thread &caller, Obj &to_destroy)
:
	_caller(caller), _obj_to_destroy(to_destroy)
{
	_obj_to_destroy->_cpu().work_list().insert(&_le);
	_caller._become_inactive(AWAITS_RESTART);
}


template <typename Obj>
void Thread::Destroy<Obj>::execute(Cpu &)
{
	_obj_to_destroy->_cpu().work_list().remove(&_le);
	_obj_to_destroy.destruct();
	_caller._restart();
}

template class Thread::Destroy<Thread>;
template class Thread::Destroy<Vcpu>;


void Thread_fault::print(Genode::Output &out) const
{
	Genode::print(out, "ip=",          Genode::Hex(ip));
	Genode::print(out, " fault-addr=", Genode::Hex(addr));
	Genode::print(out, " type=");
	switch (type) {
		case WRITE:        Genode::print(out, "write-fault"); return;
		case EXEC:         Genode::print(out, "exec-fault"); return;
		case PAGE_MISSING: Genode::print(out, "no-page"); return;
		case UNKNOWN:      Genode::print(out, "unknown"); return;
	};
}


void Thread::signal_context_kill_done()
{
	assert(_state == AWAITS_SIGNAL_CONTEXT_KILL);
	_become_active();
}


void Thread::signal_receive_signal(void * const base, size_t const size)
{
	Genode::memcpy(utcb()->data(), base, size);
	_become_active();
}


void Thread::ipc_send_request_succeeded()
{
	assert(_state == AWAITS_IPC);
	_become_active();
	helping_finished();
}


void Thread::ipc_send_request_failed()
{
	assert(_state == AWAITS_IPC);
	_become_inactive(DEAD);
	helping_finished();
}


void Thread::ipc_await_request_succeeded()
{
	assert(_state == AWAITS_IPC);
	_become_active();
}


void Thread::_become_active()
{
	if (_state == DEAD)
		return;

	if (_state != ACTIVE && !_paused) Cpu_context::_activate();
	_state = ACTIVE;
}


void Thread::_become_inactive(State const s)
{
	if (_state == DEAD)
		return;

	if ((_state == ACTIVE && !_paused) || (s == DEAD))
		Cpu_context::_deactivate();
	_state = s;
}


Rpc_result Thread::_call_thread_start(Thread &thread, Native_utcb &utcb)
{
	assert(thread._state == AWAITS_START);

	switch (thread._ipc_init(utcb, *this)) {
	case Ipc_alloc_result::OK:
		break;
	case Ipc_alloc_result::EXHAUSTED:
		return Rpc_result::OUT_OF_CAPS;
	}

	thread._become_active();
	return Rpc_result::OK;
}


void Thread::_call_thread_pause(Thread &thread)
{
	if (thread._state == ACTIVE && !thread._paused)
		thread._deactivate();

	thread._paused = true;
}


void Thread::_call_thread_resume(Thread &thread)
{
	if (thread._state == ACTIVE && thread._paused)
		thread._activate();

	thread._paused = false;
}


void Thread::_call_thread_stop()
{
	assert(_state == ACTIVE);
	_become_inactive(AWAITS_RESTART);
}


Thread_restart_result Thread::_call_thread_restart(capid_t const id)
{
	return _pd.cap_tree().with<Thread>(id,
		[&] (auto &thread) {

			if (_type == USER && (&_pd != &thread._pd)) {
				_die("Invalid  cap ", (unsigned)id, " to restart thread");
				return Thread_restart_result::INVALID;
			}

			return ((thread._restart())
				? Thread_restart_result::RESTARTED
				: Thread_restart_result::ALREADY_ACTIVE);
		}, [] { return Thread_restart_result::INVALID; });
}


bool Thread::_restart()
{
	assert(_state == ACTIVE || _state == AWAITS_RESTART);

	if (_state == ACTIVE && _exception_state == NO_EXCEPTION)
		return false;

	_exception_state = NO_EXCEPTION;
	_become_active();
	return true;
}


void Thread::_call_thread_destroy(Core::Kernel_object<Thread> &to_delete)
{
	/**
	 * Delete a thread immediately if it is assigned to this cpu,
	 * or the assigned cpu did not scheduled it.
	 */
	if (to_delete->_cpu().id() == Cpu::executing_id() ||
	    &to_delete->_cpu().current_context() != &*to_delete) {
		to_delete.destruct();
		return;
	}

	/**
	 * Construct a cross-cpu work item and send an IPI
	 */
	_thread_destroy.construct(*this, to_delete);
	to_delete->_cpu().trigger_ip_interrupt();
}


void Thread::_call_pd_destroy(Core::Kernel_object<Pd> &pd)
{
	if (_cpu().active(pd->mmu_regs))
		_cpu().switch_to(_core_pd.mmu_regs);

	pd.destruct();
}


Rpc_result Thread::_call_rpc_wait(unsigned rcv_caps_cnt)
{
	if (!_ipc_node.ready_to_wait()) {
		_die("RPC wait called in bad state!");
		return Rpc_result::OK;
	}

	if (_ipc_alloc_recv_caps(rcv_caps_cnt) == Ipc_alloc_result::EXHAUSTED)
		return Rpc_result::OUT_OF_CAPS;

	_ipc_node.wait();
	if (_ipc_node.waiting()) _become_inactive(AWAITS_IPC);

	return Rpc_result::OK;
}


void Thread::_call_timeout(timeout_t const us, capid_t const sigid)
{
	Timer &t = _cpu().timer();
	_timeout_sigid = sigid;
	t.set_timeout(*this, t.us_to_ticks(us));
}


void Thread::timeout_triggered()
{
	_pd.cap_tree().with<Signal_context>(_timeout_sigid,
		[] (Signal_context &sc) { sc.submit(1); },
		[&] () {
			Genode::warning(*this, ": failed to submit timeout signal"); });
}


Rpc_result Thread::_call_rpc_call(capid_t const id, unsigned rcv_caps_cnt)
{
	if (!_ipc_node.ready_to_send()) {
		_die("RPC send called in bad state!");
		return Rpc_result::OK;
	}

	auto set_reply_cap = [&] (Thread &dst) {
		_pd.cap_tree().with(id,
			[&] (Object_identity_reference &oir) {
				oir.with_in_pd(dst._pd,
					[&] (auto &dst_oir) { _ipc_capid = dst_oir.capid(); },
					[&] () { _ipc_capid = cap_id_invalid(); });
			},
			[&] () { _ipc_capid = cap_id_invalid(); });
	};

	return _pd.cap_tree().with<Thread>(id,
		[&] (Thread &dst) {
			if (_ipc_alloc_recv_caps(rcv_caps_cnt) ==
			    Ipc_alloc_result::EXHAUSTED)
				return Rpc_result::OUT_OF_CAPS;

			set_reply_cap(dst);

			bool const help = Cpu_context::_helping_possible(dst);

			_ipc_node.send(dst._ipc_node);

			_state = AWAITS_IPC;

			if (help) Cpu_context::_help(dst);
			if (!help || !dst.ready()) _deactivate();
			return Rpc_result::OK;
		},
		[&] () {
			_die("RPC call cannot send to unknown recipient ", id);
			return Rpc_result::OK;
		});
}


void Thread::_call_rpc_reply()
{
	_ipc_node.reply();
}


Rpc_result Thread::_call_rpc_reply_and_wait(unsigned rcv_caps_cnt)
{
	_ipc_node.reply();
	return _call_rpc_wait(rcv_caps_cnt);
}


void Thread::_call_thread_pager(Thread &thread, Thread &pager, capid_t id)
{
	_pd.cap_tree().with<Signal_context>(id,
		[&] (Signal_context &sc) {
			thread._fault_context.construct(pager, sc); },
		[&] () {
			Genode::error("core failed to set thread's ", thread,
			              "pager to cap ", id); });
}


Signal_result Thread::_call_signal_wait(capid_t const id)
{
	return _pd.cap_tree().with<Signal_receiver>(id,
		[&] (Signal_receiver &receiver) {
			switch (receiver.add_handler(_signal_handler)) {
			case Signal_receiver::Result::WAIT:
				_become_inactive(AWAITS_SIGNAL);
				[[fallthrough]];
			case Signal_receiver::Result::DELIVERED:
				return Signal_result::OK;
			case Signal_receiver::Result::INVALID: break;
			};
			return Signal_result::INVALID;
		},
		[] () { return Signal_result::INVALID; });
}


Signal_result Thread::_call_signal_pending(capid_t const id)
{
	return _pd.cap_tree().with<Signal_receiver>(id,
		[&] (auto &receiver) {
			switch (receiver.add_handler(_signal_handler)) {
			case Signal_receiver::Result::DELIVERED:
				return Signal_result::OK;
			case Signal_receiver::Result::WAIT:
				_signal_handler.cancel_waiting();
				[[fallthrough]];
			case Signal_receiver::Result::INVALID: break;
			};
			return Signal_result::INVALID;
		},
		[] () { return Signal_result::INVALID; });
}


void Thread::_call_signal_submit(capid_t const id, unsigned count)
{
	_pd.cap_tree().with<Signal_context>(id,
		[&] (auto &context) { context.submit(count); },
		[] () { /* ignore invalid signal */ });
}


void Thread::_call_signal_ack(capid_t const id)
{
	_pd.cap_tree().with<Signal_context>(id,
		[&] (auto &context) { context.ack(); },
		[] () { /* ignore invalid signal */ });
}


void Thread::_call_signal_kill(capid_t const id)
{
	_pd.cap_tree().with<Signal_context>(id,
		[&] (auto &context) {
			if (context.kill(_signal_context_killer) ==
			    Signal_context::Kill_result::IN_DELIVERY)
				_become_inactive(AWAITS_SIGNAL_CONTEXT_KILL);
		},
		[] () { /* ignore invalid signal */ });
}


capid_t Thread::_call_irq_create(Core::Kernel_object<User_irq> &kobj,
                                 unsigned                       number,
                                 Genode::Irq_session::Trigger   trigger,
                                 Genode::Irq_session::Polarity  polarity,
                                 capid_t id)
{
	return _pd.cap_tree().with<Signal_context>(id,
		[&] (auto &context) {
			kobj.construct(_core_pd, number, trigger, polarity, context,
			               _cpu().pic(), _user_irq_pool);
			return kobj->core_capid(); },
		[] () { return cap_id_invalid(); });
}


capid_t Thread::_call_obj_create(Thread_identity &kobj, capid_t id)
{
	return _pd.cap_tree().with<Thread>(id,
		[&] (auto &thread) {
			return _pd.cap_tree().with(id,
				[&] (auto &oir) {
					if (static_cast<Core_object<Thread>&>(thread).capid() != oir.capid())
						return cap_id_invalid();
					kobj.construct(_core_pd, thread);
					return kobj->core_capid();
				},
				[] { return cap_id_invalid(); });
		},
		[] { return cap_id_invalid(); });
}


void Thread::_call_cap_ack(capid_t const id)
{
	_pd.cap_tree().with(id,
		[&] (auto &oir) { oir.remove_from_utcb(); },
		[] () { /* ignore invalid cap */ });
}


void Thread::_call_cap_destroy(capid_t const id)
{
	_pd.cap_tree().with(id,
		[&] (auto &oir) {
			if (!oir.in_utcb()) destroy(_pd.cap_slab(), &oir); },
		[] () { /* ignore invalid cap */ });
}


void Kernel::Thread::_call_pd_invalidate_tlb(Pd &pd, addr_t const addr,
                                             size_t const size)
{
	unsigned cnt = 0;

	_cpu_pool.for_each_cpu([&] (Cpu &cpu) {
		/* if a cpu needs to update increase the counter */
		if (pd.invalidate_tlb(cpu, addr, size)) cnt++; });

	/* insert the work item in the list if there are outstanding cpus */
	if (cnt) {
		_tlb_invalidation.construct(
			_cpu_pool.work_list(), *this, pd, addr, size, cnt);
	}
}


void Thread::_call_thread_pager_signal_ack(capid_t id, Thread &thread, bool resolved)
{
	_pd.cap_tree().with<Signal_context>(id,
		[&] (Signal_context &context) { context.ack(); },
		[] () { /* ignore invalid pager signal */ });

	thread.helping_finished();

	bool restart = resolved || thread._exception_state == NO_EXCEPTION;
	if (restart) thread._restart();
	else         thread._become_inactive(AWAITS_RESTART);
}



void Thread::_call()
{
	/* switch over unrestricted kernel calls */
	switch (user_arg_0<Call_id>()) {
	case Call_id::CACHE_CLEAN_INV:
		{
			_call_cache_clean_invalidate(user_arg_1<addr_t>(),
			                             user_arg_2<size_t>());
			return;
		}
	case Call_id::CACHE_COHERENT:
		{
			_call_cache_coherent(user_arg_1<addr_t>(),
			                     user_arg_2<size_t>());
			return;
		}
	case Call_id::CACHE_INV:
		{
			_call_cache_invalidate(user_arg_1<addr_t>(),
			                       user_arg_2<size_t>());
			return;
		}
	case Call_id::CACHE_SIZE:
		{
			user_ret(_call_cache_line_size());
			return;
		}
	case Call_id::CAP_ACK:
		{
			_call_cap_ack(user_arg_1<capid_t>());
			return;
		}
	case Call_id::CAP_DESTROY:
		{
			_call_cap_destroy(user_arg_1<capid_t>());
			return;
		}
	case Call_id::PRINT:
		{
			Kernel::log(user_arg_1<char>());
			return;
		}
	case Call_id::RPC_CALL:
		{
			user_ret(_call_rpc_call(user_arg_1<capid_t>(),
			                        user_arg_2<unsigned>()));
			return;
		}
	case Call_id::RPC_REPLY:
		{
			_call_rpc_reply();
			return;
		}
	case Call_id::RPC_REPLY_AND_WAIT:
		{
			user_ret(_call_rpc_reply_and_wait(user_arg_1<unsigned>()));
			return;
		}
	case Call_id::RPC_WAIT:
		{
			user_ret(_call_rpc_wait(user_arg_1<unsigned>()));
			return;
		}
	case Call_id::SIG_ACK:
		{
			_call_signal_ack(user_arg_1<capid_t>());
			return;
		}
	case Call_id::SIG_KILL:
		{
			_call_signal_kill(user_arg_1<capid_t>());
			return;
		}
	case Call_id::SIG_PENDING:
		{
			user_ret(_call_signal_pending(user_arg_1<capid_t>()));
			return;
		}
	case Call_id::SIG_SUBMIT:
		{
			_call_signal_submit(user_arg_1<capid_t>(),
			                    user_arg_2<unsigned>());
			return;
		}
	case Call_id::SIG_WAIT:
		{
			user_ret(_call_signal_wait(user_arg_1<capid_t>()));
			return;
		}
	case Call_id::THREAD_RESTART:
		{
			user_ret(_call_thread_restart(user_arg_1<capid_t>()));
			return;
		}
	case Call_id::THREAD_STOP:
		{
			_call_thread_stop();
			return;
		}
	case Call_id::THREAD_YIELD:
		{
			Cpu_context::_yield();
			return;
		}
	case Call_id::TIME:
		{
			user_ret_time(_cpu().timer().ticks_to_us(_cpu().timer().time()));
			return;
		}
	case Call_id::TIMEOUT:
		{
			_call_timeout(user_arg_1<timeout_t>(), user_arg_2<capid_t>());
			return;
		}
	case Call_id::TIMEOUT_MAX_US:
		{
			user_ret_time(_cpu().timer().timeout_max_us());
			return;
		}
	case Call_id::VCPU_PAUSE:
		{
			_call_vcpu_pause(user_arg_1<capid_t>());
			return;
		}
	case Call_id::VCPU_RUN:
		{
			_call_vcpu_run(user_arg_1<capid_t>());
			return;
		}
	default:
		/* check wether this is a core thread */
		if (_type != CORE) {
			_die("Invalid system call ", user_arg_0<unsigned>());
			return;
		}
	}

	/* switch over kernel calls that are restricted to core */
	switch (user_arg_0<Core_call_id>()) {
	case Core_call_id::CPU_SUSPEND:
		{
			user_ret(_call_cpu_suspend(user_arg_1<unsigned>()));
			return;
		}
	case Core_call_id::IRQ_ACK:
		{
			user_arg_1<User_irq*>()->enable();
			return;
		}
	case Core_call_id::IRQ_CREATE:
		{
			user_ret(_call_irq_create(*user_arg_1<C_irq*>(),
			                          user_arg_2<unsigned>(),
			                          user_arg_3<Genode::Irq_session::Trigger>(),
			                          user_arg_4<Genode::Irq_session::Polarity>(),
			                          user_arg_5<capid_t>()));
			return;
		}
	case Core_call_id::IRQ_DESTROY:
		{
			_call_destruct<User_irq>();
			return;
		}
	case Core_call_id::OBJECT_CREATE:
		{
			user_ret(_call_obj_create(*user_arg_1<Thread_identity*>(),
			                          user_arg_2<capid_t>()));
			return;
		}
	case Core_call_id::OBJECT_DESTROY:
		{
			user_arg_1<Thread_identity*>()->destruct();
			return;
		}
	case Core_call_id::PD_CREATE:
		{
			_call_create<Pd>(*user_arg_2<Pd::Core_pd_data*>(),
			                 _addr_space_id_alloc);
			return;
		}
	case Core_call_id::PD_DESTROY:
		{
			_call_destruct<Pd>();
			return;
		}
	case Core_call_id::PD_INVALIDATE_TLB:
		{
			_call_pd_invalidate_tlb(*user_arg_1<Pd*>(), user_arg_2<addr_t>(),
			                        user_arg_3<size_t>());
			return;
		}
	case Core_call_id::SIGNAL_CONTEXT_CREATE:
		{
			_call_create<Signal_context>(*user_arg_2<Signal_receiver*>(),
			                             user_arg_3<addr_t>());
			return;
		}
	case Core_call_id::SIGNAL_CONTEXT_DESTROY:
		{
			_call_destruct<Signal_context>();
			return;
		}
	case Core_call_id::SIGNAL_RECEIVER_CREATE:
		{
			_call_create<Signal_receiver>();
			return;
		}
	case Core_call_id::SIGNAL_RECEIVER_DESTROY:
		{
			_call_destruct<Signal_receiver>();
			return;
		}
	case Core_call_id::THREAD_CREATE:
		{
			_cpu_pool.with_cpu(user_arg_3<unsigned>(), [&] (Cpu &cpu) {
				_call_create<Thread>(_addr_space_id_alloc, _user_irq_pool,
				                     _cpu_pool, cpu, _core_pd,
				                     *user_arg_2<Pd*>(),
				                     Scheduler::Group_id(user_arg_4<unsigned>()),
				                     user_arg_5<char const*>(), USER);
			});
			return;
		}
	case Core_call_id::THREAD_CORE_CREATE:
		{
			_cpu_pool.with_cpu(user_arg_2<unsigned>(), [&] (Cpu &cpu) {
				_call_create<Thread>(_addr_space_id_alloc, _user_irq_pool,
				                     _cpu_pool, cpu, _core_pd,
				                     user_arg_3<char const*>());
			});
			return;
		}
	case Core_call_id::THREAD_CPU_STATE_GET:
		{
			*user_arg_2<Cpu_state*>() = *user_arg_1<Thread*>()->regs;
			return;
		}
	case Core_call_id::THREAD_CPU_STATE_SET:
		{
			static_cast<Cpu_state&>(*user_arg_1<Thread*>()->regs) =
				*user_arg_2<Cpu_state*>();
			return;
		}
	case Core_call_id::THREAD_DESTROY:
		{
			_call_thread_destroy(*user_arg_1<C_thread*>());
			return;
		}
	case Core_call_id::THREAD_EXC_STATE_GET:
		{
			*user_arg_2<Exception_state*>() =
				user_arg_1<Thread*>()->exception_state();
			return;
		}
	case Core_call_id::THREAD_PAGER_SET:
		{
			_call_thread_pager(*user_arg_1<Thread*>(), *user_arg_2<Thread*>(),
			                   user_arg_3<capid_t>());
			return;
		}
	case Core_call_id::THREAD_PAGER_SIGNAL_ACK:
		{
			_call_thread_pager_signal_ack(user_arg_1<capid_t>(),
			                              *user_arg_2<Thread*>(),
			                              user_arg_3<bool>());
			return;
		}
	case Core_call_id::THREAD_PAUSE:
		{
			_call_thread_pause(*user_arg_1<Thread*>());
			return;
		}
	case Core_call_id::THREAD_RESUME:
		{
			_call_thread_resume(*user_arg_1<Thread*>());
			return;
		}
	case Core_call_id::THREAD_SINGLE_STEP:
		{
			Cpu::single_step(*user_arg_1<Thread*>()->regs, user_arg_2<bool>());
			return;
		}
	case Core_call_id::THREAD_START:
		{
			user_ret(_call_thread_start(*user_arg_1<Thread*>(),
			                            *user_arg_2<Native_utcb*>()));
			return;
		}
	case Core_call_id::VCPU_CREATE:
		{
			user_ret(_call_vcpu_create(*user_arg_1<C_vcpu*>(), user_arg_2<unsigned>(),
			                           *user_arg_3<Board::Vcpu_state*>(),
			                           *user_arg_4<Vcpu::Identity*>(),
			                           user_arg_5<capid_t>()));
			return;
		}
	case Core_call_id::VCPU_DESTROY:
		{
			_call_destruct<Vcpu>();
			return;
		}
	default:
		_die("CRITICAL: invalid system call ", user_arg_0<unsigned>());
		return;
	}
}


void Thread::_signal_to_pager()
{
	if (!_fault_context.constructed()) {
		_die("Could not send signal to pager");
		return;
	}

	/* first signal to pager to wake it up */
	_fault_context->sc.submit(1);

	/* only help pager thread if runnable and scheduler allows it */
	bool const help = Cpu_context::_helping_possible(_fault_context->pager)
	                  && (_fault_context->pager._state == ACTIVE);
	if (help) Cpu_context::_help(_fault_context->pager);
	else _become_inactive(AWAITS_RESTART);
}


void Thread::_mmu_exception()
{
	using namespace Genode;
	using Genode::log;

	_exception_state = MMU_FAULT;
	Cpu::mmu_fault(*regs, _fault);
	_fault.ip = regs->ip;

	if (_fault.type == Thread_fault::UNKNOWN) {
		_die("Unable to handle MMU fault: ", _fault);
		return;
	}

	if (_type != USER) {
		error("Core/kernel raised a fault, which should never happen ",
		      _fault);
		log("Register dump: ", *regs);
		log("Backtrace:");

		Const_byte_range_ptr const stack {
			(char const*)Hw::Mm::core_stack_area().base,
			 Hw::Mm::core_stack_area().size };
		regs->for_each_return_address(stack, [&] (void **p) {
			log(*p); });
		_die("Unable to resolve!");
		return;
	}

	_signal_to_pager();
}


void Thread::_exception()
{
	_exception_state = EXCEPTION;

	if (_type != USER)
		_die("Core/kernel raised an exception, which should never happen");

	_signal_to_pager();
}


Thread::Thread(Board::Address_space_id_allocator &addr_space_id_alloc,
               Irq::Pool                         &user_irq_pool,
               Cpu_pool                          &cpu_pool,
               Cpu                               &cpu,
               Pd                                &core_pd,
               Pd                                &pd,
               Scheduler::Group_id         const  group_id,
               char                 const *const  label,
               Type                               type)
:
	Kernel::Object       { *this },
	Cpu_context          { cpu, group_id },
	_addr_space_id_alloc { addr_space_id_alloc },
	_user_irq_pool       { user_irq_pool },
	_cpu_pool            { cpu_pool },
	_core_pd             { core_pd },
	_ipc_node            { *this },
	_pd                  { pd },
	_state               { AWAITS_START },
	_label               { label },
	_type                { type },
	regs                 { type != USER }
{ }


Thread::~Thread() { _ipc_free_recv_caps(); }


void Thread::print(Genode::Output &out) const
{
	Genode::print(out, _pd);
	Genode::print(out, " -> ");
	Genode::print(out, label());
}


Genode::uint8_t __initial_stack_base[DEFAULT_STACK_SIZE];


/**********************
 ** Core_main_thread **
 **********************/

Core_main_thread::
Core_main_thread(Board::Address_space_id_allocator &addr_space_id_alloc,
                 Irq::Pool                         &user_irq_pool,
                 Cpu_pool                          &cpu_pool,
                 Pd                                &core_pd)
:
	Core_object<Thread>(core_pd, addr_space_id_alloc, user_irq_pool, cpu_pool,
	                    cpu_pool.primary_cpu(), core_pd, "core")
{
	using namespace Core;

	map_local(Core::Platform::core_phys_addr((addr_t)&_utcb_instance),
	          (addr_t)utcb_main_thread(),
	          sizeof(Native_utcb) / get_page_size());

	_utcb_instance.cap_add(core_capid());
	_utcb_instance.cap_add(cap_id_invalid());
	_utcb_instance.cap_add(cap_id_invalid());

	/* start thread with stack pointer at the top of stack */
	regs->sp = (addr_t)&__initial_stack_base[0] + DEFAULT_STACK_SIZE;
	regs->ip = (addr_t)&_core_start;

	_utcb = &_utcb_instance;
	_become_active();
}
