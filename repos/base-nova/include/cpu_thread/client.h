/*
 * \brief  Client-side CPU thread interface
 * \author Norman Feske
 * \date   2016-05-10
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__CPU_THREAD__CLIENT_H_
#define _INCLUDE__CPU_THREAD__CLIENT_H_

#include <cpu_thread/cpu_thread.h>
#include <base/rpc_client.h>

namespace Genode { struct Cpu_thread_client; }


struct Genode::Cpu_thread_client : Rpc_client<Cpu_thread>
{
	explicit Cpu_thread_client(Thread_capability cap)
	: Rpc_client<Cpu_thread>(cap) { }

	Dataspace_capability utcb() override {
		return call<Rpc_utcb>(); }

	void start(addr_t ip, addr_t sp) override {
		call<Rpc_start>(ip, sp); }

	void pause() override {

		for (;;) {

			call<Rpc_pause>();

			try {
				/* check if the thread state is valid */
				state();
				/* the thread is paused */
				return;
			} catch (State_access_failed) {
				/* the thread is (most likely) running on a different CPU */
			}
		}
	}

	void resume() override {
		call<Rpc_resume>(); }

	void cancel_blocking() override {
		call<Rpc_cancel_blocking>(); }

	Thread_state state() override {
		return call<Rpc_get_state>(); }

	void state(Thread_state const &state) override {
		call<Rpc_set_state>(state); }

	void exception_sigh(Signal_context_capability handler) override {
		call<Rpc_exception_sigh>(handler); }

	void single_step(bool enabled) override {
		call<Rpc_single_step>(enabled); }

	void affinity(Affinity::Location location) override {
		call<Rpc_affinity>(location); }

	unsigned trace_control_index() override {
		return call<Rpc_trace_control_index>(); }

	Dataspace_capability trace_buffer() override {
		return call<Rpc_trace_buffer>(); }

	Dataspace_capability trace_policy() override {
		return call<Rpc_trace_policy>(); }
};

#endif /* _INCLUDE__CPU_THREAD__CLIENT_H_ */
