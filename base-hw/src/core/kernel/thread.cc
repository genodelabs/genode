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

/* base-hw includes */
#include <placement_new.h>

/* core includes */
#include <kernel/kernel.h>
#include <kernel/thread.h>
#include <kernel/vm.h>
#include <platform_thread.h>
#include <platform_pd.h>

using namespace Kernel;

typedef Genode::Thread_state Thread_state;

unsigned Thread::pd_id() const { return _pd ? _pd->id() : 0; }

bool Thread::_core() const { return pd_id() == core_id(); }


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
		_schedule();
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
		_schedule();
		return;
	case SCHEDULED:
		PERR("failed to receive IPC");
		_stop();
		return;
	default:
		PERR("wrong thread state to cancel IPC");
		_stop();
		return;
	}
}


int Thread::_resume()
{
	switch (_state) {
	case AWAITS_RESUME:
		_schedule();
		return 0;
	case SCHEDULED:
		return 1;
	case AWAITS_IPC:
		Ipc_node::cancel_waiting();
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


Thread::Thread(Platform_thread * const pt)
:
	Execution_context(pt ? pt->priority() : Priority::MAX),
	Thread_cpu_support(this),
	_platform_thread(pt),
	_state(AWAITS_START),
	_pd(0),
	_phys_utcb(0),
	_virt_utcb(0),
	_signal_receiver(0)
{ }


void
Thread::init(void * const ip, void * const sp, unsigned const cpu_id,
             unsigned const pd_id_arg, Native_utcb * const utcb_phys,
             Native_utcb * const utcb_virt, bool const main,
             bool const start)
{
	assert(_state == AWAITS_START)

	/* FIXME: support SMP */
	if (cpu_id) { PERR("multicore processing not supported"); }

	/* store thread parameters */
	_phys_utcb = utcb_phys;
	_virt_utcb = utcb_virt;

	/* join protection domain */
	_pd = Pd::pool()->object(pd_id_arg);
	assert(_pd);
	addr_t const tlb = _pd->tlb()->base();

	/* initialize CPU context */
	User_context * const c = static_cast<User_context *>(this);
	if (!main) { c->init_thread(ip, sp, tlb, pd_id()); }
	else if (!_core()) { c->init_main_thread(ip, utcb_virt, tlb, pd_id()); }
	else { c->init_core_main_thread(ip, sp, tlb, pd_id()); }

	/* print log message */
	if (START_VERBOSE) {
		PINF("in program %u '%s' start thread %u '%s'",
		     pd_id(), pd_label(), id(), label());
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
		_call();
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
	if (!_pd) { return "?"; }
	return _pd->platform_pd()->label();
}


void Thread::_call_new_pd()
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


void Thread::_call_kill_pd()
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


void Thread::_call_new_thread()
{
	/* check permissions */
	assert(_core());

	/* dispatch arguments */
	Call_arg const arg1 = user_arg_1();
	Call_arg const arg2 = user_arg_2();

	/* create thread */
	Thread * const t = new ((void *)arg1)
	Thread((Platform_thread *)arg2);

	/* return thread ID */
	user_arg_0((Call_ret)t->id());
}


void Thread::_call_delete_thread()
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


void Thread::_call_start_thread()
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
	user_arg_0((Call_ret)t->_pd->tlb());
}


void Thread::_call_pause_thread()
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


void Thread::_call_resume_thread()
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


void Thread_event::_signal_acknowledged()
{
	Cpu::tlb_insertions();
	_thread->_resume();
}


Thread_event::Thread_event(Thread * const t)
:
	_thread(t), _signal_context(0)
{ }


void Thread_event::submit()
{
	if (_signal_context && !_signal_context->submit(1)) { return; }
	PERR("failed to communicate thread event");
}


void Thread::_call_yield_thread()
{
	Thread * const t = Thread::pool()->object(user_arg_1());
	if (t) { t->_receive_yielded_cpu(); }
	cpu_scheduler()->yield();
}


void Thread::_call_current_thread_id() { user_arg_0((Call_ret)id()); }


void Thread::_call_get_thread()
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
	user_arg_0((Call_ret)t->platform_thread());
}


void Thread::_call_wait_for_request()
{
	void * buf_base;
	size_t buf_size;
	_phys_utcb->call_wait_for_request(buf_base, buf_size);
	Ipc_node::await_request(buf_base, buf_size);
}


void Thread::_call_request_and_wait()
{
	Thread * const dst = Thread::pool()->object(user_arg_1());
	if (!dst) {
		PERR("unkonwn recipient");
		_await_ipc();
		return;
	}
	void * msg_base;
	size_t msg_size;
	void * buf_base;
	size_t buf_size;
	_phys_utcb->call_request_and_wait(msg_base, msg_size,
	                                     buf_base, buf_size);
	Ipc_node::send_request_await_reply(dst, msg_base, msg_size,
	                                   buf_base, buf_size);
}


void Thread::_call_reply()
{
	void * msg_base;
	size_t msg_size;
	_phys_utcb->call_reply(msg_base, msg_size);
	Ipc_node::send_reply(msg_base, msg_size);
	bool const await_request = user_arg_1();
	if (await_request) { _call_wait_for_request(); }
}


void Thread::_call_route_thread_event()
{
	/* check permissions */
	if (!_core()) {
		PERR("not entitled to route thread event");
		user_arg_0(-1);
		return;
	}
	/* get targeted thread */
	unsigned const thread_id = user_arg_1();
	Thread * const t = Thread::pool()->object(thread_id);
	if (!t) {
		PERR("unknown thread");
		user_arg_0(-1);
		return;
	}

	/* override event route */
	unsigned const event_id = user_arg_2();
	unsigned const signal_context_id = user_arg_3();
	if (t->_route_event(event_id, signal_context_id)) { user_arg_0(-1); }
	else { user_arg_0(0); }
	return;
}


int Thread::_route_event(unsigned const event_id,
                         unsigned const signal_context_id)
{
	/* lookup signal context */
	Signal_context * c;
	if (signal_context_id) {
		c = Signal_context::pool()->object(signal_context_id);
		if (!c) {
			PERR("unknown signal context");
			return -1;
		}
	} else { c = 0; }

	/* lookup event and assign signal context */
	Thread_event Thread::* e = _event(event_id);
	if (!e) { return -1; }
	(this->*e).signal_context(c);
	return 0;
}


void Thread_event::signal_context(Signal_context * const c)
{
	_signal_context = c;
	if (_signal_context) { _signal_context->ack_handler(this); }
}


unsigned Thread_event::signal_context_id() const
{
	if (_signal_context) { return _signal_context->id(); }
	return 0;
}


void Thread::_call_access_thread_regs()
{
	/* check permissions */
	if (!_core()) {
		PERR("not entitled to access thread regs");
		user_arg_0(-1);
		return;
	}
	/* get targeted thread */
	unsigned const thread_id = user_arg_1();
	Thread * const t = Thread::pool()->object(thread_id);
	if (!t) {
		PERR("unknown thread");
		user_arg_0(-1);
		return;
	}
	/* execute read operations */
	unsigned const reads = user_arg_2();
	unsigned const writes = user_arg_3();
	addr_t * const utcb = (addr_t *)_phys_utcb->base();
	addr_t * const read_ids = &utcb[0];
	addr_t * const read_values = (addr_t *)user_arg_4();
	for (unsigned i = 0; i < reads; i++) {
		if (t->_read_reg(read_ids[i], read_values[i])) {
			user_arg_0(reads + writes - i);
			return;
		}
	}
	/* execute write operations */
	addr_t * const write_ids = &utcb[reads];
	addr_t * const write_values = (addr_t *)user_arg_5();
	for (unsigned i = 0; i < writes; i++) {
		if (t->_write_reg(write_ids[i], write_values[i])) {
			user_arg_0(writes - i);
			return;
		}
	}
	user_arg_0(0);
	return;
}


void Thread::_call_update_pd()
{
	assert(_core());
	Cpu::flush_tlb_by_pid(user_arg_1());
}


void Thread::_call_update_region()
{
	assert(_core());

	/* FIXME we don't handle instruction caches by now */
	Cpu::flush_data_cache_by_virt_region((addr_t)user_arg_1(),
	                                     (size_t)user_arg_2());
}


void Thread::_call_print_char()
{
	Genode::printf("%c", (char)user_arg_1());
}


void Thread::_call_new_signal_receiver()
{
	/* check permissions */
	if (!_core()) {
		PERR("not entitled to create signal receiver");
		user_arg_0(0);
		return;
	}
	/* create receiver */
	void * const p = (void *)user_arg_1();
	Signal_receiver * const r = new (p) Signal_receiver();
	user_arg_0(r->id());
}


void Thread::_call_new_signal_context()
{
	/* check permissions */
	if (!_core()) {
		PERR("not entitled to create signal context");
		user_arg_0(0);
		return;
	}
	/* lookup receiver */
	unsigned const id = user_arg_2();
	Signal_receiver * const r = Signal_receiver::pool()->object(id);
	if (!r) {
		PERR("unknown signal receiver");
		user_arg_0(0);
		return;
	}
	/* create and assign context*/
	void * const p = (void *)user_arg_1();
	unsigned const imprint = user_arg_3();
	try {
		Signal_context * const c = new (p) Signal_context(r, imprint);
		user_arg_0(c->id());
	} catch (Signal_context::Assign_to_receiver_failed) {
		PERR("failed to assign context to receiver");
		user_arg_0(0);
	}
}


void Thread::_call_await_signal()
{
	/* check wether to acknowledge a context */
	unsigned const context_id = user_arg_2();
	if (context_id) {
		Signal_context * const c = Signal_context::pool()->object(context_id);
		if (c) { c->ack(); }
		else { PERR("failed to acknowledge signal context"); }
	}
	/* lookup receiver */
	unsigned const receiver_id = user_arg_1();
	Signal_receiver * const r = Signal_receiver::pool()->object(receiver_id);
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


void Thread::_call_signal_pending()
{
	/* lookup signal receiver */
	unsigned const id = user_arg_1();
	Signal_receiver * const r = Signal_receiver::pool()->object(id);
	if (!r) {
		PERR("unknown signal receiver");
		user_arg_0(0);
		return;
	}
	/* get pending state */
	user_arg_0(r->deliverable());
}


void Thread::_call_submit_signal()
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


void Thread::_call_ack_signal()
{
	/* lookup signal context */
	unsigned const id = user_arg_1();
	Signal_context * const c = Signal_context::pool()->object(id);
	if (!c) {
		PERR("unknown signal context");
		return;
	}
	/* acknowledge */
	c->ack();
}


void Thread::_call_kill_signal_context()
{
	/* check permissions */
	if (!_core()) {
		PERR("not entitled to kill signal context");
		user_arg_0(-1);
		return;
	}
	/* lookup signal context */
	unsigned const id = user_arg_1();
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


void Thread::_call_kill_signal_receiver()
{
	/* check permissions */
	if (!_core()) {
		PERR("not entitled to kill signal receiver");
		user_arg_0(-1);
		return;
	}
	/* lookup signal receiver */
	unsigned const id = user_arg_1();
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


void Thread::_call_new_vm()
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
	user_arg_0((Call_ret)vm->id());
}


void Thread::_call_run_vm()
{
	/* check permissions */
	assert(_core());

	/* get targeted vm via its id */
	Vm * const vm = Vm::pool()->object(user_arg_1());
	assert(vm);

	/* run targeted vm */
	vm->run();
}


void Thread::_call_pause_vm()
{
	/* check permissions */
	assert(_core());

	/* get targeted vm via its id */
	Vm * const vm = Vm::pool()->object(user_arg_1());
	assert(vm);

	/* pause targeted vm */
	vm->pause();
}


int Thread::_read_reg(addr_t const id, addr_t & value) const
{
	addr_t Thread::* const reg = _reg(id);
	if (reg) {
		value = this->*reg;
		return 0;
	}
	PERR("unknown thread register");
	return -1;
}


int Thread::_write_reg(addr_t const id, addr_t const value)
{
	addr_t Thread::* const reg = _reg(id);
	if (reg) {
		this->*reg = value;
		return 0;
	}
	PERR("unknown thread register");
	return -1;
}


void Thread::_call()
{
	switch (user_arg_0()) {
	case Call_id::NEW_THREAD:           _call_new_thread(); return;
	case Call_id::DELETE_THREAD:        _call_delete_thread(); return;
	case Call_id::START_THREAD:         _call_start_thread(); return;
	case Call_id::PAUSE_THREAD:         _call_pause_thread(); return;
	case Call_id::RESUME_THREAD:        _call_resume_thread(); return;
	case Call_id::GET_THREAD:           _call_get_thread(); return;
	case Call_id::CURRENT_THREAD_ID:    _call_current_thread_id(); return;
	case Call_id::YIELD_THREAD:         _call_yield_thread(); return;
	case Call_id::REQUEST_AND_WAIT:     _call_request_and_wait(); return;
	case Call_id::REPLY:                _call_reply(); return;
	case Call_id::WAIT_FOR_REQUEST:     _call_wait_for_request(); return;
	case Call_id::UPDATE_PD:            _call_update_pd(); return;
	case Call_id::UPDATE_REGION:        _call_update_region(); return;
	case Call_id::NEW_PD:               _call_new_pd(); return;
	case Call_id::PRINT_CHAR:           _call_print_char(); return;
	case Call_id::NEW_SIGNAL_RECEIVER:  _call_new_signal_receiver(); return;
	case Call_id::NEW_SIGNAL_CONTEXT:   _call_new_signal_context(); return;
	case Call_id::KILL_SIGNAL_CONTEXT:  _call_kill_signal_context(); return;
	case Call_id::KILL_SIGNAL_RECEIVER: _call_kill_signal_receiver(); return;
	case Call_id::AWAIT_SIGNAL:         _call_await_signal(); return;
	case Call_id::SUBMIT_SIGNAL:        _call_submit_signal(); return;
	case Call_id::SIGNAL_PENDING:       _call_signal_pending(); return;
	case Call_id::ACK_SIGNAL:           _call_ack_signal(); return;
	case Call_id::NEW_VM:               _call_new_vm(); return;
	case Call_id::RUN_VM:               _call_run_vm(); return;
	case Call_id::PAUSE_VM:             _call_pause_vm(); return;
	case Call_id::KILL_PD:              _call_kill_pd(); return;
	case Call_id::ACCESS_THREAD_REGS:   _call_access_thread_regs(); return;
	case Call_id::ROUTE_THREAD_EVENT:   _call_route_thread_event(); return;
	default:
		PERR("unkonwn kernel call");
		_stop();
		reset_lap_time();
	}
}
