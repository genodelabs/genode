/*
 * \brief  Client-side CPU session interface
 * \author Norman Feske
 * \date   2012-08-09
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__LINUX_CPU_SESSION__CLIENT_H_
#define _INCLUDE__LINUX_CPU_SESSION__CLIENT_H_

#include <linux_cpu_session/linux_cpu_session.h>
#include <base/rpc_client.h>

namespace Genode {

	struct Linux_cpu_session_client : Rpc_client<Linux_cpu_session>
	{
		explicit Linux_cpu_session_client(Capability<Linux_cpu_session> session)
		: Rpc_client<Linux_cpu_session>(session) { }

		Thread_capability create_thread(size_t weight, Name const &name, addr_t utcb = 0) {
			return call<Rpc_create_thread>(weight, name, utcb); }

		Ram_dataspace_capability utcb(Thread_capability thread) {
			return call<Rpc_utcb>(thread); }

		void kill_thread(Thread_capability thread) {
			call<Rpc_kill_thread>(thread); }

		int set_pager(Thread_capability thread, Pager_capability pager) {
			return call<Rpc_set_pager>(thread, pager); }

		int start(Thread_capability thread, addr_t ip, addr_t sp) {
			return call<Rpc_start>(thread, ip, sp); }

		void pause(Thread_capability thread) {
			call<Rpc_pause>(thread); }

		void resume(Thread_capability thread) {
			call<Rpc_resume>(thread); }

		void cancel_blocking(Thread_capability thread) {
			call<Rpc_cancel_blocking>(thread); }

		Thread_state state(Thread_capability thread) {
			return call<Rpc_get_state>(thread); }

		void state(Thread_capability thread, Thread_state const &state) {
			call<Rpc_set_state>(thread, state); }

		void exception_handler(Thread_capability thread, Signal_context_capability handler) {
			call<Rpc_exception_handler>(thread, handler); }

		void single_step(Thread_capability thread, bool enable) {
			call<Rpc_single_step>(thread, enable); }

		Affinity::Space affinity_space() const {
			return call<Rpc_affinity_space>(); }

		void affinity(Thread_capability thread, Affinity::Location location) {
			call<Rpc_affinity>(thread, location); }

		Dataspace_capability trace_control() {
			return call<Rpc_trace_control>(); }

		unsigned trace_control_index(Thread_capability thread) {
			return call<Rpc_trace_control_index>(thread); }

		Dataspace_capability trace_buffer(Thread_capability thread) {
			return call<Rpc_trace_buffer>(thread); }

		Dataspace_capability trace_policy(Thread_capability thread) {
			return call<Rpc_trace_policy>(thread); }

		int ref_account(Cpu_session_capability session) {
			return call<Rpc_ref_account>(session); }

		int transfer_quota(Cpu_session_capability session, size_t amount) {
			return call<Rpc_transfer_quota>(session, amount); }

		Quota quota() override { return call<Rpc_quota>(); }

		/*****************************
		 * Linux-specific extension **
		 *****************************/

		void thread_id(Thread_capability thread, int pid, int tid) {
			call<Rpc_thread_id>(thread, pid, tid); }

		Untyped_capability server_sd(Thread_capability thread) {
			return call<Rpc_server_sd>(thread); }

		Untyped_capability client_sd(Thread_capability thread) {
			return call<Rpc_client_sd>(thread); }
	};
}

#endif /* _INCLUDE__LINUX_CPU_SESSION__CLIENT_H_ */
