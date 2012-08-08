/*
 * \brief  Client-side cpu session NOVA extension
 * \author Alexander Boettcher
 * \date   2012-07-27
 */

/*
 * Copyright (C) 2012-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__NOVA_CPU_SESSION__CLIENT_H_
#define _INCLUDE__NOVA_CPU_SESSION__CLIENT_H_

#include <cpu_session/client.h>
#include <nova_cpu_session/nova_cpu_session.h>

namespace Genode {

	struct Nova_cpu_session_client : Rpc_client<Nova_cpu_session>
	{
		explicit Nova_cpu_session_client(Cpu_session_capability session)
		: Rpc_client<Nova_cpu_session>(static_cap_cast<Nova_cpu_session>(session)) { }

		Thread_capability create_thread(Name const &name, addr_t utcb = 0) {
			return call<Rpc_create_thread>(name, utcb); }

		Ram_dataspace_capability utcb(Thread_capability thread) {
			return call<Rpc_utcb>(thread); }


		void kill_thread(Thread_capability thread) {
			call<Rpc_kill_thread>(thread); }

		Thread_capability first() {
			return call<Rpc_first>(); }

		Thread_capability next(Thread_capability curr) {
			return call<Rpc_next>(curr); }

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

		int state(Thread_capability thread, Thread_state *dst_state) {
			return call<Rpc_state>(thread, dst_state); }

		void exception_handler(Thread_capability thread, Signal_context_capability handler) {
			call<Rpc_exception_handler>(thread, handler); }

		void single_step(Thread_capability thread, bool enable) {
			call<Rpc_single_step>(thread, enable); }

		Native_capability native_cap(Thread_capability cap) {
			return call<Rpc_native_cap>(cap); }

		int start_exc_base_vcpu(Thread_capability thread, addr_t ip,
		                        addr_t sp, addr_t exc_base,
		                        bool vcpu = false)
		{
			return call<Rpc_start_exc_base_vcpu>(thread, ip, sp,
			                                     exc_base, vcpu);
		}


	};
}

#endif /* _INCLUDE__NOVA_CPU_SESSION__CLIENT_H_ */
