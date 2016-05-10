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

namespace Genode { struct Cpu_session_client; }


struct Genode::Cpu_session_client : Rpc_client<Nova_cpu_session>
{
	explicit Cpu_session_client(Cpu_session_capability session)
	: Rpc_client<Nova_cpu_session>(static_cap_cast<Nova_cpu_session>(session)) { }

	Thread_capability
	create_thread(Capability<Pd_session> pd, Name const &name,
	              Affinity::Location affinity, Weight weight, addr_t utcb = 0) override {
		return call<Rpc_create_thread>(pd, name, affinity, weight, utcb); }

	void kill_thread(Thread_capability thread) override {
		call<Rpc_kill_thread>(thread); }

	void exception_sigh(Signal_context_capability handler) override {
		call<Rpc_exception_sigh>(handler); }

	Affinity::Space affinity_space() const override {
		return call<Rpc_affinity_space>(); }

	Dataspace_capability trace_control() override {
		return call<Rpc_trace_control>(); }

	int ref_account(Cpu_session_capability session) override {
		return call<Rpc_ref_account>(session); }

	int transfer_quota(Cpu_session_capability session, size_t amount) override {
		return call<Rpc_transfer_quota>(session, amount); }

	Quota quota() override { return call<Rpc_quota>(); }

	Capability<Native_cpu> native_cpu() override { return call<Rpc_native_cpu>(); }

	private:

		Native_capability pause_sync(Thread_capability) {
			return Native_capability(); }

		Native_capability single_step_sync(Thread_capability, bool) {
			return Native_capability(); }
};

#endif /* _INCLUDE__CPU_SESSION__CLIENT_H_ */
