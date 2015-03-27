/*
 * \brief  Client-side cpu session interface
 * \author Christian Helmuth
 * \date   2006-07-12
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__CPU_SESSION__CLIENT_H_
#define _INCLUDE__CPU_SESSION__CLIENT_H_

#include <cpu_session/capability.h>
#include <base/rpc_client.h>

namespace Genode { struct Cpu_session_client; }


struct Genode::Cpu_session_client : Rpc_client<Cpu_session>
{
	explicit Cpu_session_client(Cpu_session_capability session)
	: Rpc_client<Cpu_session>(session) { }

	Thread_capability
	create_thread(size_t quota, Name const &name, addr_t utcb = 0) override {
		return call<Rpc_create_thread>(quota, name, utcb); }

	Ram_dataspace_capability utcb(Thread_capability thread) override {
		return call<Rpc_utcb>(thread); }

	void kill_thread(Thread_capability thread) override {
		call<Rpc_kill_thread>(thread); }

	int set_pager(Thread_capability thread, Pager_capability pager) override {
		return call<Rpc_set_pager>(thread, pager); }

	int start(Thread_capability thread, addr_t ip, addr_t sp) override {
		return call<Rpc_start>(thread, ip, sp); }

	void pause(Thread_capability thread) override {
		call<Rpc_pause>(thread); }

	void resume(Thread_capability thread) override {
		call<Rpc_resume>(thread); }

	void cancel_blocking(Thread_capability thread) override {
		call<Rpc_cancel_blocking>(thread); }

	Thread_state state(Thread_capability thread) override {
		return call<Rpc_get_state>(thread); }

	void state(Thread_capability thread, Thread_state const &state) override {
		call<Rpc_set_state>(thread, state); }

	void exception_handler(Thread_capability thread, Signal_context_capability handler) override {
		call<Rpc_exception_handler>(thread, handler); }

	void single_step(Thread_capability thread, bool enable) override {
		call<Rpc_single_step>(thread, enable); }

	Affinity::Space affinity_space() const override {
		return call<Rpc_affinity_space>(); }

	void affinity(Thread_capability thread, Affinity::Location location) override {
		call<Rpc_affinity>(thread, location); }

	Dataspace_capability trace_control() override {
		return call<Rpc_trace_control>(); }

	unsigned trace_control_index(Thread_capability thread) override {
		return call<Rpc_trace_control_index>(thread); }

	Dataspace_capability trace_buffer(Thread_capability thread) override {
		return call<Rpc_trace_buffer>(thread); }

	Dataspace_capability trace_policy(Thread_capability thread) override {
		return call<Rpc_trace_policy>(thread); }

	int ref_account(Cpu_session_capability session) override {
		return call<Rpc_ref_account>(session); }

	int transfer_quota(Cpu_session_capability session, size_t amount) override {
		return call<Rpc_transfer_quota>(session, amount); }

	Quota quota() override { return call<Rpc_quota>(); }
};

#endif /* _INCLUDE__CPU_SESSION__CLIENT_H_ */
