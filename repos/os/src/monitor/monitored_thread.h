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
#include <types.h>

namespace Monitor { struct Monitored_thread; }


struct Monitor::Monitored_thread : Monitored_rpc_object<Cpu_thread>
{
	static void with_thread(Entrypoint &ep, Capability<Cpu_thread> cap,
	                        auto const &monitored_fn, auto const &direct_fn)
	{
		with_monitored<Monitored_thread>(ep, cap, monitored_fn, direct_fn);
	}

	Threads::Element _threads_elem;

	using Monitored_rpc_object::Monitored_rpc_object;

	Monitored_thread(Entrypoint &ep, Capability<Cpu_thread> real, Name const &name,
	                 Threads &threads, Threads::Id id)
	:
		Monitored_rpc_object(ep, real, name), _threads_elem(*this, threads, id)
	{ }

	long unsigned id() const { return _threads_elem.id().value; }

	Dataspace_capability utcb() override {
		return _real.call<Rpc_utcb>(); }

	void start(addr_t ip, addr_t sp) override {
		_real.call<Rpc_start>(ip, sp); }

	void pause() override {
		_real.call<Rpc_pause>(); }

	void resume() override {
		_real.call<Rpc_resume>(); }

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
