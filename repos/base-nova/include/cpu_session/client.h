/*
 * \brief  Client-side cpu session NOVA extension
 * \author Alexander Boettcher
 * \date   2012-07-27
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__CPU_SESSION__CLIENT_H_
#define _INCLUDE__CPU_SESSION__CLIENT_H_

#include <base/rpc_client.h>
#include <base/thread.h>

#include <cpu_session/capability.h>

#include <nova_cpu_session/nova_cpu_session.h>

namespace Genode {

	struct Cpu_session_client : Rpc_client<Nova_cpu_session>
	{
		explicit Cpu_session_client(Cpu_session_capability session)
		: Rpc_client<Nova_cpu_session>(static_cap_cast<Nova_cpu_session>(session)) { }

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

		void pause(Thread_capability thread)
		{
			Native_capability block = call<Rpc_pause_sync>(thread);
			if (!block.valid())
				return;

			Nova::sm_ctrl(block.local_name(), Nova::SEMAPHORE_DOWN);
		}

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

		void single_step(Thread_capability thread, bool enable)
		{
			Native_capability block = call<Rpc_single_step_sync>(thread, enable);
			if (!block.valid())
				return;

			Nova::sm_ctrl(block.local_name(), Nova::SEMAPHORE_DOWN);
		}

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

		private:

			Native_capability pause_sync(Thread_capability) {
				return Native_capability(); }

			Native_capability single_step_sync(Thread_capability, bool) {
				return Native_capability(); }
	};
}

#endif /* _INCLUDE__CPU_SESSION__CLIENT_H_ */
