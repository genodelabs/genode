/*
 * \brief  Client-side cpu session Fiasco.OC extension
 * \author Stefan Kalkowski
 * \date   2011-04-04
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__FOC_CPU_SESSION__CLIENT_H_
#define _INCLUDE__FOC_CPU_SESSION__CLIENT_H_

#include <cpu_session/client.h>
#include <foc_cpu_session/foc_cpu_session.h>
#include <base/printf.h>

namespace Genode {

	struct Foc_cpu_session_client : Rpc_client<Foc_cpu_session>
	{
		explicit Foc_cpu_session_client(Cpu_session_capability session)
		: Rpc_client<Foc_cpu_session>(static_cap_cast<Foc_cpu_session>(session)) { }

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

		int name(Thread_capability thread, char *name_dst, size_t name_len)
		{
			PWRN("name called, this function is deprecated");
			return -1;
		}

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

		void enable_vcpu(Thread_capability cap, addr_t vcpu_state) {
			call<Rpc_enable_vcpu>(cap, vcpu_state); }

		Native_capability native_cap(Thread_capability cap) {
			return call<Rpc_native_cap>(cap); }

		Native_capability alloc_irq() {
			return call<Rpc_alloc_irq>(); }

		int ref_account(Cpu_session_capability session) {
			return call<Rpc_ref_account>(session); }

		int transfer_quota(Cpu_session_capability session, size_t amount) {
			return call<Rpc_transfer_quota>(session, amount); }

		Quota quota() override { return call<Rpc_quota>(); }
	};

}

#endif /* _INCLUDE__FOC_CPU_SESSION__CLIENT_H_ */
