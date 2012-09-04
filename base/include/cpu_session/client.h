/*
 * \brief  Client-side cpu session interface
 * \author Christian Helmuth
 * \date   2006-07-12
 */

/*
 * Copyright (C) 2006-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__CPU_SESSION__CLIENT_H_
#define _INCLUDE__CPU_SESSION__CLIENT_H_

#include <cpu_session/capability.h>
#include <base/rpc_client.h>

namespace Genode {

	struct Cpu_session_client : Rpc_client<Cpu_session>
	{
		explicit Cpu_session_client(Cpu_session_capability session)
		: Rpc_client<Cpu_session>(session) { }

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

		unsigned num_cpus() const {
			return call<Rpc_num_cpus>(); }

		void affinity(Thread_capability thread, unsigned cpu) {
			call<Rpc_affinity>(thread, cpu); }
	};
}

#endif /* _INCLUDE__CPU_SESSION__CLIENT_H_ */
