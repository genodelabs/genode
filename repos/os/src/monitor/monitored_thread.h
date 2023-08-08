/*
 * \brief  Monitored CPU thread
 * \author Norman Feske
 * \date   2023-05-16
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MONITORED_THREAD_H_
#define _MONITORED_THREAD_H_

/* Genode includes */
#include <base/rpc_client.h>
#include <cpu_thread/client.h>

/* local includes */
#include <gdb_arch.h>
#include <types.h>

namespace Monitor {
	struct Monitored_thread;
	struct Thread_monitor;
}


/*
 * Interface for the interaction of the monitored thread
 * with the monitor.
 */
struct Monitor::Thread_monitor : Interface
{
	virtual void set_initial_breakpoint(Capability<Pd_session> pd,
	                                    addr_t addr,
	                                    char original_instruction[]) = 0;

	virtual void remove_initial_breakpoint(Capability<Pd_session> pd,
	                                       addr_t addr,
	                                       char const original_instruction[]) = 0;

	virtual void flush(Monitored_thread &thread) = 0;

	virtual void thread_stopped(Capability<Pd_session> pd,
	                            Monitored_thread &thread) = 0;
};


struct Monitor::Monitored_thread : Monitored_rpc_object<Cpu_thread>
{
	static void with_thread(Entrypoint &ep, Capability<Cpu_thread> cap,
	                        auto const &monitored_fn, auto const &direct_fn)
	{
		with_monitored<Monitored_thread>(ep, cap, monitored_fn, direct_fn);
	}

	Threads::Element        _threads_elem;
	Capability<Pd_session>  _pd;
	Thread_monitor         &_thread_monitor;
	bool                    _wait;

	addr_t _first_instruction_addr { };
	char   _original_first_instruction[Gdb::max_breakpoint_instruction_len] { };

	Signal_handler<Monitored_thread> _exception_handler;

	/* stop reply signal values as expected by GDB */
	enum Stop_reply_signal {
		STOP = 0,
		ILL  = 4,
		TRAP = 5,
		FPE  = 8,
		SEGV = 11
	};

	Stop_reply_signal stop_reply_signal { STOP };

	enum Stop_state {
		RUNNING,
		STOPPED_REPLY_PENDING,
		STOPPED_REPLY_SENT,
		STOPPED_REPLY_ACKED
	};

	Stop_state stop_state { RUNNING };

	void _handle_exception();

	void handle_page_fault()
	{
		/*
		 * On NOVA 'pause()' must be called to get the
		 * complete register state.
		 */
		pause();

		stop_state = Stop_state::STOPPED_REPLY_PENDING;
		stop_reply_signal = Stop_reply_signal::SEGV;

		_thread_monitor.thread_stopped(_pd, *this);
	}

	using Monitored_rpc_object::Monitored_rpc_object;

	Monitored_thread(Entrypoint &ep, Capability<Cpu_thread> real, Name const &name,
	                 Threads &threads, Threads::Id id,
	                 Capability<Pd_session> pd,
	                 Thread_monitor &thread_monitor, bool wait)
	:
		Monitored_rpc_object(ep, real, name),
		_threads_elem(*this, threads, id),
		_pd(pd), _thread_monitor(thread_monitor), _wait(wait),
		_exception_handler(ep, *this, &Monitored_thread::_handle_exception)
	{
		_real.call<Rpc_exception_sigh>(_exception_handler);

	}

	~Monitored_thread()
	{
		_thread_monitor.flush(*this);
	}

	long unsigned id() const { return _threads_elem.id().value; }

	Dataspace_capability utcb() override {
		return _real.call<Rpc_utcb>(); }

	void start(addr_t ip, addr_t sp) override
	{
		if (_wait) {
			_first_instruction_addr = ip;
			_thread_monitor.set_initial_breakpoint(_pd, ip,
			                                       _original_first_instruction);
		}

		_real.call<Rpc_start>(ip, sp);
	}

	void pause() override
	{
		_real.call<Rpc_pause>();
		stop_state = Stop_state::STOPPED_REPLY_PENDING;
		stop_reply_signal = Stop_reply_signal::STOP;
	}

	void resume() override {
		stop_state = Stop_state::RUNNING;
		_real.call<Rpc_resume>();
	}

	Thread_state state() override {
		return _real.call<Rpc_get_state>(); }

	void state(Thread_state const &state) override {
		_real.call<Rpc_set_state>(state); }

	void exception_sigh(Signal_context_capability handler) override {
		_real.call<Rpc_exception_sigh>(handler); }

	void single_step(bool enabled) override {
		_real.call<Rpc_single_step>(enabled); }

	void affinity(Affinity::Location location) override {
		_real.call<Rpc_affinity>(location); }

	unsigned trace_control_index() override {
		return _real.call<Rpc_trace_control_index>(); }

	Dataspace_capability trace_buffer() override {
		return _real.call<Rpc_trace_buffer>(); }

	Dataspace_capability trace_policy() override {
		return _real.call<Rpc_trace_policy>(); }
};

#endif /* _MONITORED_THREAD_H_ */
