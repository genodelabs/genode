/*
 * \brief  Kernel backend for execution contexts in userland
 * \author Martin Stein
 * \date   2013-09-15
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/thread_state.h>

/* core includes */
#include <kernel/kernel.h>
#include <kernel/thread.h>
#include <kernel/vm.h>
#include <platform_thread.h>
#include <platform_pd.h>

using namespace Kernel;

typedef Genode::Thread_state Thread_state;


bool Thread::_core() const
{
	return pd_id() == core_id();
}


Kernel::Pd * Thread::_pd() const
{
	return Pd::pool()->object(pd_id());
}


void Thread::_signal_context_kill_pending()
{
	assert(_state == SCHEDULED);
	_state = AWAITS_SIGNAL_CONTEXT_KILL;
	cpu_scheduler()->remove(this);
}


void Thread::_signal_context_kill_done()
{
	assert(_state == AWAITS_SIGNAL_CONTEXT_KILL);
	user_arg_0(0);
	_schedule();
}


void Thread::_signal_receiver_kill_pending()
{
	assert(_state == SCHEDULED);
	_state = AWAITS_SIGNAL_RECEIVER_KILL;
	cpu_scheduler()->remove(this);
}


void Thread::_signal_receiver_kill_done()
{
	assert(_state == AWAITS_SIGNAL_RECEIVER_KILL);
	user_arg_0(0);
	_schedule();
}


void Thread::_await_signal(Signal_receiver * const receiver)
{
	cpu_scheduler()->remove(this);
	_state = AWAITS_SIGNAL;
	_signal_receiver = receiver;
}


void Thread::_receive_signal(void * const base, size_t const size)
{
	assert(_state == AWAITS_SIGNAL && size <= _phys_utcb->size());
	Genode::memcpy(_phys_utcb->base(), base, size);
	_schedule();
}


void Thread::_received_ipc_request(size_t const s)
{
	switch (_state) {
	case SCHEDULED:
		_phys_utcb->ipc_msg.size = s;
		user_arg_0(0);
		return;
	default:
		PERR("wrong thread state to receive IPC");
		_stop();
		return;
	}
}


void Thread::_await_ipc()
{
	switch (_state) {
	case SCHEDULED:
		cpu_scheduler()->remove(this);
		_state = AWAITS_IPC;
	case AWAITS_PAGER:
		return;
	default:
		PERR("wrong thread state to await IPC");
		_stop();
		return;
	}
}


void Thread::_await_ipc_succeeded(size_t const s)
{
	switch (_state) {
	case AWAITS_IPC:
		_phys_utcb->ipc_msg.size = s;
		user_arg_0(0);
		_schedule();
		return;
	case AWAITS_PAGER_IPC:
		_schedule();
		return;
	case AWAITS_PAGER:
		_state = AWAITS_RESUME;
		return;
	default:
		PERR("wrong thread state to receive IPC");
		_stop();
		return;
	}
}


void Thread::_await_ipc_failed()
{
	switch (_state) {
	case AWAITS_IPC:
		user_arg_0(-1);
		_schedule();
		return;
	case SCHEDULED:
		PERR("failed to receive IPC");
		_stop();
		return;
	case AWAITS_PAGER_IPC:
		PERR("failed to get pagefault resolved");
		_stop();
		return;
	case AWAITS_PAGER:
		PERR("failed to get pagefault resolved");
		_stop();
		return;
	default:
		PERR("wrong thread state to cancel IPC");
		_stop();
		return;
	}
}


void Thread::_received_irq()
{
	assert(_state == AWAITS_IRQ);
	_schedule();
}


void Thread::_awaits_irq()
{
	cpu_scheduler()->remove(this);
	_state = AWAITS_IRQ;
}


int Thread::_resume()
{
	switch (_state) {
	case AWAITS_RESUME:
		_schedule();
		return 0;
	case AWAITS_PAGER:
		_state = AWAITS_PAGER_IPC;
		return 0;
	case AWAITS_PAGER_IPC:
		Ipc_node::cancel_waiting();
		return 0;
	case SCHEDULED:
		return 1;
	case AWAITS_IPC:
		Ipc_node::cancel_waiting();
		return 0;
	case AWAITS_IRQ:
		Irq_receiver::cancel_waiting();
		return 0;
	case AWAITS_SIGNAL:
		Signal_handler::cancel_waiting();
		return 0;
	case AWAITS_SIGNAL_CONTEXT_KILL:
		Signal_context_killer::cancel_waiting();
		return 0;
	case AWAITS_SIGNAL_RECEIVER_KILL:
		Signal_receiver_killer::cancel_waiting();
		return 0;
	case AWAITS_START:
	case STOPPED:;
	}
	PERR("failed to resume thread");
	return -1;
}


void Thread::_pause()
{
	assert(_state == AWAITS_RESUME || _state == SCHEDULED);
	cpu_scheduler()->remove(this);
	_state = AWAITS_RESUME;
}


void Thread::_schedule()
{
	cpu_scheduler()->insert(this);
	_state = SCHEDULED;
}


Thread::Thread(Platform_thread * const platform_thread)
:
	_platform_thread(platform_thread), _state(AWAITS_START),
	_pager(0), _pd_id(0), _phys_utcb(0), _virt_utcb(0),
	_signal_receiver(0)
{
	if (_platform_thread) { priority = _platform_thread->priority(); }
	else { priority = Kernel::Priority::MAX; }
}


void
Thread::init(void * const ip, void * const sp, unsigned const cpu_id,
             unsigned const pd_id, Native_utcb * const utcb_phys,
             Native_utcb * const utcb_virt, bool const main,
             bool const start)
{
	assert(_state == AWAITS_START)

	/* FIXME: support SMP */
	if (cpu_id) { PERR("multicore processing not supported"); }

	/* store thread parameters */
	_phys_utcb = utcb_phys;
	_virt_utcb = utcb_virt;
	_pd_id     = pd_id;

	/* join protection domain */
	Pd * const pd = Pd::pool()->object(_pd_id);
	assert(pd);
	addr_t const tlb = pd->tlb()->base();

	/* initialize CPU context */
	User_context * const c = static_cast<User_context *>(this);
	if (!main) { c->init_thread(ip, sp, tlb, pd_id); }
	else if (!_core()) { c->init_main_thread(ip, utcb_virt, tlb, pd_id); }
	else { c->init_core_main_thread(ip, sp, tlb, pd_id); }

	/* print log message */
	if (START_VERBOSE) {
		PINF("in program %u '%s' start thread %u '%s'",
		     this->pd_id(), pd_label(), id(), label());
	}
	/* start execution */
	if (start) { _schedule(); }
}


void Thread::_stop()
{
	if (_state == SCHEDULED) { cpu_scheduler()->remove(this); }
	_state = STOPPED;
}


void Thread::handle_exception()
{
	switch (cpu_exception) {
	case SUPERVISOR_CALL:
		_syscall();
		return;
	case PREFETCH_ABORT:
		_mmu_exception();
		return;
	case DATA_ABORT:
		_mmu_exception();
		return;
	case INTERRUPT_REQUEST:
		handle_interrupt();
		return;
	case FAST_INTERRUPT_REQUEST:
		handle_interrupt();
		return;
	default:
		PERR("unknown exception");
		_stop();
		reset_lap_time();
	}
}


void Thread::_receive_yielded_cpu()
{
	if (_state == AWAITS_RESUME) { _schedule(); }
	else { PERR("failed to receive yielded CPU"); }
}


void Thread::proceed()
{
	mtc()->continue_user(static_cast<Cpu::Context *>(this));
}


void Thread::_mmu_exception()
{
	/* pause thread */
	cpu_scheduler()->remove(this);
	_state = AWAITS_PAGER;

	/* check out cause and attributes */
	addr_t va = 0;
	bool   w  = 0;
	if (!pagefault(va, w)) {
		PERR("unknown MMU exception");
		return;
	}
	/* inform pager */
	_pagefault = Pagefault(id(), (Tlb *)tlb(), ip, va, w);
	void * const base = &_pagefault;
	size_t const size = sizeof(_pagefault);
	Ipc_node::send_request_await_reply(_pager, base, size, base, size);
}


char const * Kernel::Thread::label() const
{
	if (!platform_thread()) {
		if (!_phys_utcb) { return "idle"; }
		return "core";
	}
	return platform_thread()->name();
}


char const * Kernel::Thread::pd_label() const
{
	if (_core()) { return "core"; }
	if (!_pd()) { return "?"; }
	return _pd()->platform_pd()->label();
}


/**
 * Do specific syscall for this thread, for details see 'syscall.h'
 */
void Thread::_syscall_new_pd()
{
	/* check permissions */
	if (!_core()) {
		PERR("not entitled to create protection domain");
		user_arg_0(0);
		return;
	}
	/* create translation lookaside buffer and protection domain */
	void * p = (void *)user_arg_1();
	Tlb * const tlb = new (p) Tlb();
	p = (void *)((addr_t)p + sizeof(Tlb));
	Pd * const pd = new (p) Pd(tlb, (Platform_pd *)user_arg_2());
	user_arg_0(pd->id());
}


/**
 * Do specific syscall for this thread, for details see 'syscall.h'
 */
void Thread::_syscall_kill_pd()
{
	/* check permissions */
	if (!_core()) {
		PERR("not entitled to destruct protection domain");
		user_arg_0(-1);
		return;
	}
	/* lookup protection domain */
	unsigned id = user_arg_1();
	Pd * const pd = Pd::pool()->object(id);
	if (!pd) {
		PERR("unknown protection domain");
		user_arg_0(-1);
		return;
	}
	/* destruct translation lookaside buffer and protection domain */
	Tlb * const tlb = pd->tlb();
	pd->~Pd();
	tlb->~Tlb();

	/* clean up buffers of memory management */
	Cpu::flush_tlb_by_pid(pd->id());
	user_arg_0(0);
}


/**
 * Do specific syscall for this thread, for details see 'syscall.h'
 */
void Thread::_syscall_new_thread()
{
	/* check permissions */
	assert(_core());

	/* dispatch arguments */
	Syscall_arg const arg1 = user_arg_1();
	Syscall_arg const arg2 = user_arg_2();

	/* create thread */
	Thread * const t = new ((void *)arg1)
	Thread((Platform_thread *)arg2);

	/* return thread ID */
	user_arg_0((Syscall_ret)t->id());
}

/**
 * Do specific syscall for this thread, for details see 'syscall.h'
 */
void Thread::_syscall_delete_thread()
{
	/* check permissions */
	assert(_core());

	/* get targeted thread */
	unsigned thread_id = (unsigned)user_arg_1();
	Thread * const thread = Thread::pool()->object(thread_id);
	assert(thread);

	/* destroy thread */
	thread->~Thread();
}

/**
 * Do specific syscall for this thread, for details see 'syscall.h'
 */
void Thread::_syscall_start_thread()
{
	/* check permissions */
	assert(_core());

	/* dispatch arguments */
	Platform_thread * pt = (Platform_thread *)user_arg_1();
	void * const ip = (void *)user_arg_2();
	void * const sp = (void *)user_arg_3();
	unsigned const cpu_id = (unsigned)user_arg_4();

	/* get targeted thread */
	Thread * const t = Thread::pool()->object(pt->id());
	assert(t);

	/* start thread */
	unsigned const pd_id = pt->pd_id();
	Native_utcb * const utcb_p = pt->phys_utcb();
	Native_utcb * const utcb_v = pt->virt_utcb();
	bool const main = pt->main_thread();
	t->init(ip, sp, cpu_id, pd_id, utcb_p, utcb_v, main, 1);

	/* return software TLB that the thread is assigned to */
	Pd::Pool * const pp = Pd::pool();
	Pd * const pd = pp->object(t->pd_id());
	assert(pd);
	user_arg_0((Syscall_ret)pd->tlb());
}


/**
 * Do specific syscall for this thread, for details see 'syscall.h'
 */
void Thread::_syscall_pause_thread()
{
	unsigned const tid = user_arg_1();

	/* shortcut for a thread to pause itself */
	if (!tid) {
		_pause();
		user_arg_0(0);
		return;
	}

	/* get targeted thread and check permissions */
	Thread * const t = Thread::pool()->object(tid);
	assert(t && (_core() || this == t));

	/* pause targeted thread */
	t->_pause();
	user_arg_0(0);
}


/**
 * Do specific syscall for this thread, for details see 'syscall.h'
 */
void Thread::_syscall_resume_thread()
{
	/* lookup thread */
	Thread * const t = Thread::pool()->object(user_arg_1());
	if (!t) {
		PERR("unknown thread");
		user_arg_0(-1);
		return;
	}
	/* check permissions */
	if (!_core() && pd_id() != t->pd_id()) {
		PERR("not entitled to resume thread");
		user_arg_0(-1);
		return;
	}
	/* resume targeted thread */
	user_arg_0(t->_resume());
}


/**
 * Do specific syscall for this thread, for details see 'syscall.h'
 */
void Thread::_syscall_resume_faulter()
{
	/* lookup thread */
	Thread * const t = Thread::pool()->object(user_arg_1());
	if (!t) {
		PERR("unknown thread");
		user_arg_0(-1);
		return;
	}
	/* check permissions */
	if (!_core() && pd_id() != t->pd_id()) {
		PERR("not entitled to resume thread");
		user_arg_0(-1);
		return;
	}
	/* writeback translation table and resume faulter */
	Cpu::tlb_insertions();
	t->_resume();
}


/**
 * Do specific syscall for this thread, for details see 'syscall.h'
 */
void Thread::_syscall_yield_thread()
{
	Thread * const t = Thread::pool()->object(user_arg_1());
	if (t) { t->_receive_yielded_cpu(); }
	cpu_scheduler()->yield();
}


/**
 * Do specific syscall for this thread, for details see 'syscall.h'
 */
void Thread::_syscall_current_thread_id()
{ user_arg_0((Syscall_ret)id()); }


/**
 * Do specific syscall for this thread, for details see 'syscall.h'
 */
void Thread::_syscall_get_thread()
{
	/* check permissions */
	if (!_core()) {
		PERR("not entitled to read address of platform thread");
		user_arg_0(0);
		return;
	}
	/* lookup thread */
	unsigned const id = user_arg_1();
	Thread * t;
	if (id) {
		t = Thread::pool()->object(id);
		if (!t) {
			PERR("unknown thread");
			user_arg_0(0);
		}
	} else { t = this; }
	user_arg_0((Syscall_ret)t->platform_thread());
}


/**
 * Do specific syscall for this thread, for details see 'syscall.h'
 */
void Thread::_syscall_wait_for_request()
{
	void * const buf_base = _phys_utcb->ipc_msg.data;
	size_t const buf_size = _phys_utcb->ipc_msg_max_size();
	Ipc_node::await_request(buf_base, buf_size);
}


/**
 * Do specific syscall for this thread, for details see 'syscall.h'
 */
void Thread::_syscall_request_and_wait()
{
	Thread * const dst = Thread::pool()->object(user_arg_1());
	if (!dst) {
		PERR("unkonwn recipient");
		_await_ipc();
		return;
	}
	Ipc_node::send_request_await_reply(
		dst, _phys_utcb->ipc_msg.data, _phys_utcb->ipc_msg.size,
		_phys_utcb->ipc_msg.data, _phys_utcb->ipc_msg_max_size());
}


/**
 * Do specific syscall for this thread, for details see 'syscall.h'
 */
void Thread::_syscall_reply()
{
	bool const await_request = user_arg_1();
	void * const msg_base = _phys_utcb->ipc_msg.data;
	size_t const msg_size = _phys_utcb->ipc_msg.size;
	Ipc_node::send_reply(msg_base, msg_size);

	if (await_request) {
		void * const buf_base = _phys_utcb->ipc_msg.data;
		size_t const buf_size = _phys_utcb->ipc_msg_max_size();
		Ipc_node::await_request(buf_base, buf_size);
	} else { user_arg_0(0); }
}


/**
 * Do specific syscall for this thread, for details see 'syscall.h'
 */
void Thread::_syscall_set_pager()
{
	/* check permissions */
	if (!_core()) {
		PERR("not entitled to set pager");
		return;
	}
	/* lookup faulter and pager thread */
	unsigned const pager_id = user_arg_1();
	Thread * const pager    = Thread::pool()->object(pager_id);
	Thread * const faulter  = Thread::pool()->object(user_arg_2());
	if ((pager_id && !pager) || !faulter) {
		PERR("failed to set pager");
		return;
	}
	/* assign pager */
	faulter->pager(pager);
}


/**
 * Do specific syscall for this thread, for details see 'syscall.h'
 */
void Thread::_syscall_update_pd()
{
	assert(_core());
	Cpu::flush_tlb_by_pid(user_arg_1());
}


/**
 * Do specific syscall for this thread, for details see 'syscall.h'
 */
void Thread::_syscall_update_region()
{
	assert(_core());

	/* FIXME we don't handle instruction caches by now */
	Cpu::flush_data_cache_by_virt_region((addr_t)user_arg_1(),
	                                     (size_t)user_arg_2());
}


/**
 * Do specific syscall for this thread, for details see 'syscall.h'
 */
void Thread::_syscall_allocate_irq()
{
	assert(_core());
	unsigned irq = user_arg_1();
	user_arg_0(allocate_irq(irq));
}


/**
 * Do specific syscall for this thread, for details see 'syscall.h'
 */
void Thread::_syscall_free_irq()
{
	assert(_core());
	unsigned irq = user_arg_1();
	user_arg_0(free_irq(irq));
}


/**
 * Do specific syscall for this thread, for details see 'syscall.h'
 */
void Thread::_syscall_await_irq()
{
	assert(_core());
	await_irq();
}


/**
 * Do specific syscall for this thread, for details see 'syscall.h'
 */
void Thread::_syscall_print_char()
{
	Genode::printf("%c", (char)user_arg_1());
}


/**
 * Do specific syscall for this thread, for details see 'syscall.h'
 */
void Thread::_syscall_read_thread_state()
{
	assert(_core());
	Thread * const t = Thread::pool()->object(user_arg_1());
	if (!t) PDBG("Targeted thread unknown");
	Thread_state * const ts = (Thread_state *)_phys_utcb->base();
	t->Cpu::Context::read_cpu_state(ts);
}


/**
 * Do specific syscall for this thread, for details see 'syscall.h'
 */
void Thread::_syscall_write_thread_state()
{
	assert(_core());
	Thread * const t = Thread::pool()->object(user_arg_1());
	if (!t) PDBG("Targeted thread unknown");
	Thread_state * const ts = (Thread_state *)_phys_utcb->base();
	t->Cpu::Context::write_cpu_state(ts);
}


/**
 * Do specific syscall for this thread, for details see 'syscall.h'
 */
void Thread::_syscall_new_signal_receiver()
{
	/* check permissions */
	if (!_core()) {
		PERR("not entitled to create signal receiver");
		user_arg_0(0);
		return;
	}
	/* create receiver */
	void * p = (void *)user_arg_1();
	Signal_receiver * const r = new (p) Signal_receiver();
	user_arg_0(r->id());
}


/**
 * Do specific syscall for this thread, for details see 'syscall.h'
 */
void Thread::_syscall_new_signal_context()
{
	/* check permissions */
	if (!_core()) {
		PERR("not entitled to create signal context");
		user_arg_0(0);
		return;
	}
	/* lookup receiver */
	unsigned id = user_arg_2();
	Signal_receiver * const r = Signal_receiver::pool()->object(id);
	if (!r) {
		PERR("unknown signal receiver");
		user_arg_0(0);
		return;
	}
	/* create and assign context*/
	void * p = (void *)user_arg_1();
	unsigned imprint = user_arg_3();
	if (r->new_context(p, imprint)) {
		PERR("failed to create signal context");
		user_arg_0(0);
		return;
	}
	/* return context name */
	Signal_context * const c = (Signal_context *)p;
	user_arg_0(c->id());
}


/**
 * Do specific syscall for this thread, for details see 'syscall.h'
 */
void Thread::_syscall_await_signal()
{
	/* lookup receiver */
	unsigned id = user_arg_1();
	Signal_receiver * const r = Signal_receiver::pool()->object(id);
	if (!r) {
		PERR("unknown signal receiver");
		user_arg_0(-1);
		return;
	}
	/* register handler at the receiver */
	if (r->add_handler(this)) {
		PERR("failed to register handler at signal receiver");
		user_arg_0(-1);
		return;
	}
	user_arg_0(0);
}


/**
 * Do specific syscall for this thread, for details see 'syscall.h'
 */
void Thread::_syscall_signal_pending()
{
	/* lookup signal receiver */
	unsigned id = user_arg_1();
	Signal_receiver * const r = Signal_receiver::pool()->object(id);
	if (!r) {
		PERR("unknown signal receiver");
		user_arg_0(0);
		return;
	}
	/* get pending state */
	user_arg_0(r->deliverable());
}


/**
 * Do specific syscall for this thread, for details see 'syscall.h'
 */
void Thread::_syscall_submit_signal()
{
	/* lookup signal context */
	unsigned const id = user_arg_1();
	Signal_context * const c = Signal_context::pool()->object(id);
	if(!c) {
		PERR("unknown signal context");
		user_arg_0(-1);
		return;
	}
	/* trigger signal context */
	if (c->submit(user_arg_2())) {
		PERR("failed to submit signal context");
		user_arg_0(-1);
		return;
	}
	user_arg_0(0);
}


/**
 * Do specific syscall for this thread, for details see 'syscall.h'
 */
void Thread::_syscall_ack_signal()
{
	/* lookup signal context */
	unsigned id = user_arg_1();
	Signal_context * const c = Signal_context::pool()->object(id);
	if (!c) {
		PERR("unknown signal context");
		return;
	}
	/* acknowledge */
	c->ack();
}


/**
 * Do specific syscall for this thread, for details see 'syscall.h'
 */
void Thread::_syscall_kill_signal_context()
{
	/* check permissions */
	if (!_core()) {
		PERR("not entitled to kill signal context");
		user_arg_0(-1);
		return;
	}
	/* lookup signal context */
	unsigned id = user_arg_1();
	Signal_context * const c = Signal_context::pool()->object(id);
	if (!c) {
		PERR("unknown signal context");
		user_arg_0(0);
		return;
	}
	/* kill signal context */
	if (c->kill(this)) {
		PERR("failed to kill signal context");
		user_arg_0(-1);
		return;
	}
	user_arg_0(0);
}


/**
 * Do specific syscall for this thread, for details see 'syscall.h'
 */
void Thread::_syscall_kill_signal_receiver()
{
	/* check permissions */
	if (!_core()) {
		PERR("not entitled to kill signal receiver");
		user_arg_0(-1);
		return;
	}
	/* lookup signal receiver */
	unsigned id = user_arg_1();
	Signal_receiver * const r = Signal_receiver::pool()->object(id);
	if (!r) {
		PERR("unknown signal receiver");
		user_arg_0(0);
		return;
	}
	/* kill signal receiver */
	if (r->kill(this)) {
		PERR("unknown signal receiver");
		user_arg_0(-1);
		return;
	}
	user_arg_0(0);
}


/**
 * Do specific syscall for this thread, for details see 'syscall.h'
 */
void Thread::_syscall_new_vm()
{
	/* check permissions */
	assert(_core());

	/* dispatch arguments */
	void * const allocator = (void * const)user_arg_1();
	Genode::Cpu_state_modes * const state =
		(Genode::Cpu_state_modes * const)user_arg_2();
	Signal_context * const context =
		Signal_context::pool()->object(user_arg_3());
	assert(context);

	/* create vm */
	Vm * const vm = new (allocator) Vm(state, context);

	/* return vm id */
	user_arg_0((Syscall_ret)vm->id());
}


/**
 * Do specific syscall for this thread, for details see 'syscall.h'
 */
void Thread::_syscall_run_vm()
{
	/* check permissions */
	assert(_core());

	/* get targeted vm via its id */
	Vm * const vm = Vm::pool()->object(user_arg_1());
	assert(vm);

	/* run targeted vm */
	vm->run();
}


/**
 * Do specific syscall for this thread, for details see 'syscall.h'
 */
void Thread::_syscall_pause_vm()
{
	/* check permissions */
	assert(_core());

	/* get targeted vm via its id */
	Vm * const vm = Vm::pool()->object(user_arg_1());
	assert(vm);

	/* pause targeted vm */
	vm->pause();
}


/**
 * Handle a syscall request
 */
void Thread::_syscall()
{
	switch (user_arg_0())
	{
	case NEW_THREAD:           _syscall_new_thread(); return;
	case DELETE_THREAD:        _syscall_delete_thread(); return;
	case START_THREAD:         _syscall_start_thread(); return;
	case PAUSE_THREAD:         _syscall_pause_thread(); return;
	case RESUME_THREAD:        _syscall_resume_thread(); return;
	case RESUME_FAULTER:       _syscall_resume_faulter(); return;
	case GET_THREAD:           _syscall_get_thread(); return;
	case CURRENT_THREAD_ID:    _syscall_current_thread_id(); return;
	case YIELD_THREAD:         _syscall_yield_thread(); return;
	case READ_THREAD_STATE:    _syscall_read_thread_state(); return;
	case WRITE_THREAD_STATE:   _syscall_write_thread_state(); return;
	case REQUEST_AND_WAIT:     _syscall_request_and_wait(); return;
	case REPLY:                _syscall_reply(); return;
	case WAIT_FOR_REQUEST:     _syscall_wait_for_request(); return;
	case SET_PAGER:            _syscall_set_pager(); return;
	case UPDATE_PD:            _syscall_update_pd(); return;
	case UPDATE_REGION:        _syscall_update_region(); return;
	case NEW_PD:               _syscall_new_pd(); return;
	case ALLOCATE_IRQ:         _syscall_allocate_irq(); return;
	case AWAIT_IRQ:            _syscall_await_irq(); return;
	case FREE_IRQ:             _syscall_free_irq(); return;
	case PRINT_CHAR:           _syscall_print_char(); return;
	case NEW_SIGNAL_RECEIVER:  _syscall_new_signal_receiver(); return;
	case NEW_SIGNAL_CONTEXT:   _syscall_new_signal_context(); return;
	case KILL_SIGNAL_CONTEXT:  _syscall_kill_signal_context(); return;
	case KILL_SIGNAL_RECEIVER: _syscall_kill_signal_receiver(); return;
	case AWAIT_SIGNAL:         _syscall_await_signal(); return;
	case SUBMIT_SIGNAL:        _syscall_submit_signal(); return;
	case SIGNAL_PENDING:       _syscall_signal_pending(); return;
	case ACK_SIGNAL:           _syscall_ack_signal(); return;
	case NEW_VM:               _syscall_new_vm(); return;
	case RUN_VM:               _syscall_run_vm(); return;
	case PAUSE_VM:             _syscall_pause_vm(); return;
	case KILL_PD:              _syscall_kill_pd(); return;
	default:
		PERR("invalid syscall");
		_stop();
		reset_lap_time();
	}
}
