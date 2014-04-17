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
#include <kernel/irq.h>
#include <platform_pd.h>
#include <pic.h>

using namespace Kernel;

typedef Genode::Thread_state Thread_state;

unsigned Thread::pd_id() const { return _pd ? _pd->id() : 0; }

bool Thread::_core() const { return pd_id() == core_id(); }

void Thread::_signal_context_kill_pending()
{
	assert(_state == SCHEDULED);
	_unschedule(AWAITS_SIGNAL_CONTEXT_KILL);
}


void Thread::_signal_context_kill_done()
{
	assert(_state == AWAITS_SIGNAL_CONTEXT_KILL);
	user_arg_0(0);
	_schedule();
}


void Thread::_signal_context_kill_failed()
{
	assert(_state == AWAITS_SIGNAL_CONTEXT_KILL);
	user_arg_0(-1);
	_schedule();
}


void Thread::_await_signal(Signal_receiver * const receiver)
{
	_unschedule(AWAITS_SIGNAL);
	_signal_receiver = receiver;
}


void Thread::_receive_signal(void * const base, size_t const size)
{
	assert(_state == AWAITS_SIGNAL && size <= _utcb_phys->size());
	Genode::memcpy(_utcb_phys->base(), base, size);
	_schedule();
}


void Thread::_send_request_succeeded()
{
	assert(_state == AWAITS_IPC);
	user_arg_0(0);
	_schedule();
}


void Thread::_send_request_failed()
{
	assert(_state == AWAITS_IPC);
	user_arg_0(-1);
	_schedule();
}


void Thread::_await_request_succeeded()
{
	assert(_state == AWAITS_IPC);
	user_arg_0(0);
	_schedule();
}


void Thread::_await_request_failed()
{
	assert(_state == AWAITS_IPC);
	user_arg_0(-1);
	_schedule();
}


bool Thread::_resume()
{
	switch (_state) {
	case AWAITS_RESUME:
		_schedule();
		return true;
	case AWAITS_IPC:
		Ipc_node::cancel_waiting();
		return true;
	case AWAITS_SIGNAL:
		Signal_handler::cancel_waiting();
		return true;
	case AWAITS_SIGNAL_CONTEXT_KILL:
		Signal_context_killer::cancel_waiting();
		return true;
	default:
		return false;
	}
}


void Thread::_pause()
{
	assert(_state == AWAITS_RESUME || _state == SCHEDULED);
	_unschedule(AWAITS_RESUME);
}


void Thread::_schedule()
{
	if (_state == SCHEDULED) { return; }
	Processor_client::_schedule();
	_state = SCHEDULED;
}


void Thread::_unschedule(State const s)
{
	if (_state == SCHEDULED) { Processor_client::_unschedule(); }
	_state = s;
}


Thread::Thread(unsigned const priority, char const * const label)
:
	Processor_client(0, priority),
	Thread_cpu_support(this),
	_state(AWAITS_START),
	_pd(0),
	_utcb_phys(0),
	_signal_receiver(0),
	_label(label)
{
	cpu_exception = RESET;
}


void Thread::init(Processor * const processor, Pd * const pd,
             Native_utcb * const utcb_phys, bool const start)
{
	assert(_state == AWAITS_START)

	/* store thread parameters */
	Processor_client::_processor(processor);
	_utcb_phys = utcb_phys;

	/* join protection domain */
	_pd = pd;
	addr_t const tlb = _pd->tlb()->base();
	User_context::init_thread(tlb, pd_id());

	/* print log message */
	if (START_VERBOSE) {
		Genode::printf("start thread %u '%s' in program %u '%s' ",
		               id(), label(), pd_id(), pd_label());
		if (PROCESSORS) {
			Genode::printf("on processor %u/%u ",
			               processor->id(), PROCESSORS);
		}
		Genode::printf("\n");
	}
	/* start execution */
	if (start) { _schedule(); }
}


void Thread::_stop() { _unschedule(STOPPED); }


void Thread::exception(unsigned const processor_id)
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
		_interrupt(processor_id);
		return;
	case FAST_INTERRUPT_REQUEST:
		_interrupt(processor_id);
		return;
	case RESET:
		return;
	default:
		PWRN("unknown exception");
		_stop();
	}
}


void Thread::_receive_yielded_cpu()
{
	if (_state == AWAITS_RESUME) { _schedule(); }
	else { PWRN("failed to receive yielded CPU"); }
}


void Thread::proceed(unsigned const processor_id)
{
	mtc()->continue_user(this, processor_id);
}


char const * Kernel::Thread::pd_label() const
{
	if (_core()) { return "core"; }
	if (!_pd) { return "?"; }
	return _pd->platform_pd()->label();
}


void Thread::_call_new_pd()
{
	/* create translation lookaside buffer and protection domain */
	void * p = (void *)user_arg_1();
	Tlb * const tlb = new (p) Tlb();
	p = (void *)((addr_t)p + sizeof(Tlb));
	Pd * const pd = new (p) Pd(tlb, (Platform_pd *)user_arg_2());
	user_arg_0(pd->id());
}


void Thread::_call_bin_pd()
{
	/* lookup protection domain */
	unsigned id = user_arg_1();
	Pd * const pd = Pd::pool()->object(id);
	if (!pd) {
		PWRN("unknown protection domain");
		user_arg_0(-1);
		return;
	}
	/* destruct translation lookaside buffer and protection domain */
	Tlb * const tlb = pd->tlb();
	pd->~Pd();
	tlb->~Tlb();

	/* clean up buffers of memory management */
	Processor::flush_tlb_by_pid(pd->id());
	user_arg_0(0);
}


void Thread::_call_new_thread()
{
	/* create new thread */
	void * const p = (void *)user_arg_1();
	unsigned const priority = user_arg_2();
	char const * const label = (char *)user_arg_3();
	Thread * const t = new (p) Thread(priority, label);
	user_arg_0(t->id());
}


void Thread::_call_bin_thread()
{
	/* lookup thread */
	Thread * const thread = Thread::pool()->object(user_arg_1());
	if (!thread) {
		PWRN("failed to lookup thread");
		return;
	}
	/* destroy thread */
	thread->~Thread();
}


void Thread::_call_start_thread()
{
	/* lookup thread */
	unsigned const thread_id = user_arg_1();
	Thread * const thread = Thread::pool()->object(thread_id);
	if (!thread) {
		PWRN("failed to lookup  thread");
		user_arg_0(0);
		return;
	}
	/* lookup processor */
	unsigned const processor_id = user_arg_2();
	Processor * const processor = processor_pool()->processor(processor_id);
	if (!processor) {
		PWRN("failed to lookup  processor");
		user_arg_0(0);
		return;
	}
	/* lookup domain */
	unsigned const pd_id = user_arg_3();
	Pd * const pd = Pd::pool()->object(pd_id);
	if (!pd) {
		PWRN("failed to lookup domain");
		user_arg_0(0);
		return;
	}
	/* start thread */
	Native_utcb * const utcb = (Native_utcb *)user_arg_4();
	thread->init(processor, pd, utcb, 1);
	user_arg_0((Call_ret)thread->_pd->tlb());
}


void Thread::_call_pause_current_thread() { _pause(); }


void Thread::_call_pause_thread()
{
	/* lookup thread */
	Thread * const thread = Thread::pool()->object(user_arg_1());
	if (!thread) {
		PWRN("failed to lookup thread");
		return;
	}
	/* pause thread */
	thread->_pause();
}


void Thread::_call_resume_thread()
{
	/* lookup thread */
	Thread * const thread = Thread::pool()->object(user_arg_1());
	if (!thread) {
		PWRN("failed to lookup thread");
		user_arg_0(false);
		return;
	}
	/* resume thread */
	user_arg_0(thread->_resume());
}


void Thread::_call_resume_local_thread()
{
	/* lookup thread */
	Thread * const thread = Thread::pool()->object(user_arg_1());
	if (!thread || pd_id() != thread->pd_id()) {
		PWRN("failed to lookup thread");
		user_arg_0(0);
		return;
	}
	/* resume thread */
	user_arg_0(thread->_resume());
}


void Thread_event::_signal_acknowledged()
{
	Processor::tlb_insertions();
	_thread->_resume();
}


Thread_event::Thread_event(Thread * const t)
:
	_thread(t), _signal_context(0)
{ }


void Thread_event::submit()
{
	if (_signal_context && !_signal_context->submit(1)) { return; }
	PWRN("failed to communicate thread event");
}


void Thread::_call_yield_thread()
{
	Thread * const t = Thread::pool()->object(user_arg_1());
	if (t) { t->_receive_yielded_cpu(); }
	Processor_client::_yield();
}


void Thread::_call_await_request_msg()
{
	void * buf_base;
	size_t buf_size;
	_utcb_phys->message()->buffer_info(buf_base, buf_size);
	if (Ipc_node::await_request(buf_base, buf_size)) {
		user_arg_0(0);
		return;
	}
	_unschedule(AWAITS_IPC);
}


void Thread::_call_send_request_msg()
{
	Thread * const dst = Thread::pool()->object(user_arg_1());
	if (!dst) {
		PWRN("unknown recipient");
		_unschedule(AWAITS_IPC);
		return;
	}
	void * buf_base;
	size_t buf_size, msg_size;
	_utcb_phys->message()->request_info(buf_base, buf_size, msg_size);
	Ipc_node::send_request(dst, buf_base, buf_size, msg_size);
	_unschedule(AWAITS_IPC);
}


void Thread::_call_send_reply_msg()
{
	void * msg_base;
	size_t msg_size;
	_utcb_phys->message()->reply_info(msg_base, msg_size);
	Ipc_node::send_reply(msg_base, msg_size);
	bool const await_request_msg = user_arg_1();
	if (await_request_msg) { _call_await_request_msg(); }
	else { user_arg_0(0); }
}


void Thread::_call_route_thread_event()
{
	/* get targeted thread */
	unsigned const thread_id = user_arg_1();
	Thread * const t = Thread::pool()->object(thread_id);
	if (!t) {
		PWRN("unknown thread");
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
			PWRN("unknown signal context");
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
	/* get targeted thread */
	unsigned const thread_id = user_arg_1();
	unsigned const reads = user_arg_2();
	unsigned const writes = user_arg_3();
	Thread * const t = Thread::pool()->object(thread_id);
	if (!t) {
		PWRN("unknown thread");
		user_arg_0(reads + writes);
		return;
	}
	/* execute read operations */
	addr_t * const utcb = (addr_t *)_utcb_phys->base();
	addr_t * const read_ids = &utcb[0];
	addr_t * values = (addr_t *)user_arg_4();
	for (unsigned i = 0; i < reads; i++) {
		if (t->_read_reg(read_ids[i], *values)) {
			user_arg_0(reads + writes - i);
			return;
		}
		values++;
	}
	/* execute write operations */
	addr_t * const write_ids = &utcb[reads];
	for (unsigned i = 0; i < writes; i++) {
		if (t->_write_reg(write_ids[i], *values)) {
			user_arg_0(writes - i);
			return;
		}
		values++;
	}
	user_arg_0(0);
	return;
}


void Thread::_call_update_pd()
{
	/* update hardware caches */
	Processor::flush_tlb_by_pid(user_arg_1());
}


void Thread::_call_update_data_region()
{
	/*
	 * FIXME: If the caller is not a core thread, the kernel operates in a
	 *        different address space than the caller. Combined with the fact
	 *        that at least ARMv7 doesn't provide cache operations by physical
	 *        address, this prevents us from selectively maintaining caches.
	 *        The future solution will be a kernel that is mapped to every
	 *        address space so we can use virtual addresses of the caller. Up
	 *        until then we apply operations to caches as a whole instead.
	 */
	if (!_core()) {
		Processor::flush_data_caches();
		return;
	}
	auto base = (addr_t)user_arg_1();
	auto const size = (size_t)user_arg_2();
	Processor::flush_data_caches_by_virt_region(base, size);
}


void Thread::_call_update_instr_region()
{
	/*
	 * FIXME: If the caller is not a core thread, the kernel operates in a
	 *        different address space than the caller. Combined with the fact
	 *        that at least ARMv7 doesn't provide cache operations by physical
	 *        address, this prevents us from selectively maintaining caches.
	 *        The future solution will be a kernel that is mapped to every
	 *        address space so we can use virtual addresses of the caller. Up
	 *        until then we apply operations to caches as a whole instead.
	 */
	if (!_core()) {
		Processor::flush_data_caches();
		Processor::invalidate_instr_caches();
		return;
	}
	auto base = (addr_t)user_arg_1();
	auto const size = (size_t)user_arg_2();
	Processor::flush_data_caches_by_virt_region(base, size);
	Processor::invalidate_instr_caches_by_virt_region(base, size);
}


void Thread::_print_activity_table()
{
	for (unsigned id = 0; id < MAX_THREADS; id++) {
		Thread * const t = Thread::pool()->object(id);
		if (!t) { continue; }
		t->_print_activity(t == this);
	}
	return;
}


void Thread::_print_activity(bool const printing_thread)
{
	Genode::printf("\033[33m[%u] %s", pd_id(), pd_label());
	Genode::printf(" (%u) %s:\033[0m", id(), label());
	switch (_state) {
	case AWAITS_START: {
		Genode::printf("\033[32m init\033[0m");
		break; }
	case SCHEDULED: {
		if (!printing_thread) { Genode::printf("\033[32m run\033[0m"); }
		else { Genode::printf("\033[32m debug\033[0m"); }
		break; }
	case AWAITS_IPC: {
		_print_activity_when_awaits_ipc();
		break; }
	case AWAITS_RESUME: {
		Genode::printf("\033[32m await RES\033[0m");
		break; }
	case AWAITS_SIGNAL: {
		unsigned const receiver_id = Signal_handler::receiver()->id();
		Genode::printf("\033[32m await SIG %u\033[0m", receiver_id);
		break; }
	case AWAITS_SIGNAL_CONTEXT_KILL: {
		unsigned const context_id = Signal_context_killer::context()->id();
		Genode::printf("\033[32m await SCK %u\033[0m", context_id);
		break; }
	case STOPPED: {
		Genode::printf("\033[32m stop\033[0m");
		break; }
	}
	_print_common_activity();
}


void Thread::_print_common_activity()
{
	Genode::printf(" ip %lx sp %lx\n", ip, sp);
}


void Thread::_print_activity_when_awaits_ipc()
{
	switch (Ipc_node::state()) {
	case AWAIT_REPLY: {
		Thread * const server = dynamic_cast<Thread *>(Ipc_node::outbuf_dst());
		Genode::printf("\033[32m await RPL %u\033[0m", server->id());
		break; }
	case AWAIT_REQUEST: {
		Genode::printf("\033[32m await REQ\033[0m");
		break; }
	case PREPARE_AND_AWAIT_REPLY: {
		Thread * const server = dynamic_cast<Thread *>(Ipc_node::outbuf_dst());
		Genode::printf("\033[32m prep RPL await RPL %u\033[0m", server->id());
		break; }
	default: break;
	}
}


void Thread::_call_print_char()
{
	char const c = user_arg_1();
	if (!c) { _print_activity_table(); }
	Genode::printf("%c", (char)user_arg_1());
}


void Thread::_call_new_signal_receiver()
{
	/* create receiver */
	void * const p = (void *)user_arg_1();
	Signal_receiver * const r = new (p) Signal_receiver();
	user_arg_0(r->id());
}


void Thread::_call_new_signal_context()
{
	/* lookup receiver */
	unsigned const id = user_arg_2();
	Signal_receiver * const r = Signal_receiver::pool()->object(id);
	if (!r) {
		PWRN("unknown signal receiver");
		user_arg_0(0);
		return;
	}
	/* create and assign context*/
	void * const p = (void *)user_arg_1();
	unsigned const imprint = user_arg_3();
	Signal_context * const c = new (p) Signal_context(r, imprint);
	user_arg_0(c->id());
}


void Thread::_call_await_signal()
{
	/* check wether to acknowledge a context */
	unsigned const context_id = user_arg_2();
	if (context_id) {
		Signal_context * const c = Signal_context::pool()->object(context_id);
		if (c) { c->ack(); }
		else { PWRN("failed to acknowledge signal context"); }
	}
	/* lookup receiver */
	unsigned const receiver_id = user_arg_1();
	Signal_receiver * const r = Signal_receiver::pool()->object(receiver_id);
	if (!r) {
		PWRN("unknown signal receiver");
		user_arg_0(-1);
		return;
	}
	/* register handler at the receiver */
	if (r->add_handler(this)) {
		PWRN("failed to register handler at signal receiver");
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
		PWRN("unknown signal receiver");
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
		PWRN("unknown signal context");
		user_arg_0(-1);
		return;
	}
	/* trigger signal context */
	if (c->submit(user_arg_2())) {
		PWRN("failed to submit signal context");
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
		PWRN("unknown signal context");
		return;
	}
	/* acknowledge */
	c->ack();
}


void Thread::_call_kill_signal_context()
{
	/* lookup signal context */
	unsigned const id = user_arg_1();
	Signal_context * const c = Signal_context::pool()->object(id);
	if (!c) {
		PWRN("unknown signal context");
		user_arg_0(-1);
		return;
	}
	/* kill signal context */
	if (c->kill(this)) {
		PWRN("failed to kill signal context");
		user_arg_0(-1);
		return;
	}
}


void Thread::_call_bin_signal_context()
{
	/* lookup signal context */
	unsigned const id = user_arg_1();
	Signal_context * const c = Signal_context::pool()->object(id);
	if (!c) {
		PWRN("unknown signal context");
		user_arg_0(0);
		return;
	}
	/* destruct signal context */
	c->~Signal_context();
	user_arg_0(0);
}


void Thread::_call_bin_signal_receiver()
{
	/* lookup signal receiver */
	unsigned const id = user_arg_1();
	Signal_receiver * const r = Signal_receiver::pool()->object(id);
	if (!r) {
		PWRN("unknown signal receiver");
		user_arg_0(0);
		return;
	}
	r->~Signal_receiver();
	user_arg_0(0);
}


void Thread::_call_new_vm()
{
	/* lookup signal context */
	auto const context = Signal_context::pool()->object(user_arg_3());
	if (!context) {
		PWRN("failed to lookup signal context");
		user_arg_0(0);
		return;
	}
	/* create virtual machine */
	typedef Genode::Cpu_state_modes Cpu_state_modes;
	auto const allocator = reinterpret_cast<void *>(user_arg_1());
	auto const state = reinterpret_cast<Cpu_state_modes *>(user_arg_2());
	Vm * const vm = new (allocator) Vm(state, context);

	/* return kernel name of virtual machine */
	user_arg_0(vm->id());
}


void Thread::_call_run_vm()
{
	/* lookup virtual machine */
	Vm * const vm = Vm::pool()->object(user_arg_1());
	if (!vm) {
		PWRN("failed to lookup virtual machine");
		return;
	}
	/* run virtual machine */
	vm->run();
}


void Thread::_call_pause_vm()
{
	/* lookup virtual machine */
	Vm * const vm = Vm::pool()->object(user_arg_1());
	if (!vm) {
		PWRN("failed to lookup virtual machine");
		return;
	}
	/* pause virtual machine */
	vm->pause();
}


int Thread::_read_reg(addr_t const id, addr_t & value) const
{
	addr_t Thread::* const reg = _reg(id);
	if (reg) {
		value = this->*reg;
		return 0;
	}
	PWRN("unknown thread register");
	return -1;
}


int Thread::_write_reg(addr_t const id, addr_t const value)
{
	addr_t Thread::* const reg = _reg(id);
	if (reg) {
		this->*reg = value;
		return 0;
	}
	PWRN("unknown thread register");
	return -1;
}


void Thread::_call()
{
	/* switch over unrestricted kernel calls */
	unsigned const call_id = user_arg_0();
	switch (call_id) {
	case call_id_update_data_region():   _call_update_data_region(); return;
	case call_id_update_instr_region():  _call_update_instr_region(); return;
	case call_id_pause_current_thread(): _call_pause_current_thread(); return;
	case call_id_resume_local_thread():  _call_resume_local_thread(); return;
	case call_id_yield_thread():         _call_yield_thread(); return;
	case call_id_send_request_msg():     _call_send_request_msg(); return;
	case call_id_send_reply_msg():       _call_send_reply_msg(); return;
	case call_id_await_request_msg():    _call_await_request_msg(); return;
	case call_id_kill_signal_context():  _call_kill_signal_context(); return;
	case call_id_submit_signal():        _call_submit_signal(); return;
	case call_id_await_signal():         _call_await_signal(); return;
	case call_id_signal_pending():       _call_signal_pending(); return;
	case call_id_ack_signal():           _call_ack_signal(); return;
	case call_id_print_char():           _call_print_char(); return;
	default:
		/* check wether this is a core thread */
		if (!_core()) {
			PWRN("not entitled to do kernel call");
			_stop();
			return;
		}
	}
	/* switch over kernel calls that are restricted to core */
	switch (call_id) {
	case call_id_new_thread():          _call_new_thread(); return;
	case call_id_bin_thread():          _call_bin_thread(); return;
	case call_id_start_thread():        _call_start_thread(); return;
	case call_id_resume_thread():       _call_resume_thread(); return;
	case call_id_access_thread_regs():  _call_access_thread_regs(); return;
	case call_id_route_thread_event():  _call_route_thread_event(); return;
	case call_id_update_pd():           _call_update_pd(); return;
	case call_id_new_pd():              _call_new_pd(); return;
	case call_id_bin_pd():              _call_bin_pd(); return;
	case call_id_new_signal_receiver(): _call_new_signal_receiver(); return;
	case call_id_new_signal_context():  _call_new_signal_context(); return;
	case call_id_bin_signal_context():  _call_bin_signal_context(); return;
	case call_id_bin_signal_receiver(): _call_bin_signal_receiver(); return;
	case call_id_new_vm():              _call_new_vm(); return;
	case call_id_run_vm():              _call_run_vm(); return;
	case call_id_pause_vm():            _call_pause_vm(); return;
	case call_id_pause_thread():        _call_pause_thread(); return;
	default:
		PWRN("unknown kernel call");
		_stop();
		return;
	}
}
