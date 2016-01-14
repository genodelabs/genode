/*
 * \brief  Core-specific instance of the PD session interface for OKL4
 * \author Christian Helmuth
 * \author Stefan Kalkowski
 * \date   2006-07-17
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__OKL4__PD_SESSION_COMPONENT_H_
#define _CORE__INCLUDE__OKL4__PD_SESSION_COMPONENT_H_

/* Genode includes */
#include <okl4_pd_session/okl4_pd_session.h>
#include <base/rpc_server.h>

/* core includes */
#include <platform.h>
#include <signal_broker.h>

namespace Genode { class Pd_session_component; }


class Genode::Pd_session_component : public Rpc_object<Okl4_pd_session>
{
	private:

		Platform_pd     _pd;
		Rpc_entrypoint &_thread_ep;
		Signal_broker   _signal_broker;

	public:

		Pd_session_component(Rpc_entrypoint &thread_ep,
		                     Rpc_entrypoint &receiver_ep,
		                     Rpc_entrypoint &context_ep,
		                     Allocator &md_alloc, const char *args)
		:
			_thread_ep(thread_ep),
			_signal_broker(md_alloc, receiver_ep, context_ep)
		{ }

		/**
		 * Register quota donation at allocator guard
		 */
		void upgrade_ram_quota(size_t ram_quota) { }


		/**************************
		 ** Pd session interface **
		 **************************/

		int bind_thread(Thread_capability);
		int assign_parent(Capability<Parent>);
		bool assign_pci(addr_t, uint16_t) { return false; }

		Signal_source_capability alloc_signal_source() override {
			return _signal_broker.alloc_signal_source(); }

		void free_signal_source(Signal_source_capability cap) override {
			_signal_broker.free_signal_source(cap); }

		Signal_context_capability
		alloc_context(Signal_source_capability sig_rec_cap, unsigned long imprint) override
		{
			return _signal_broker.alloc_context(sig_rec_cap, imprint);
		}

		void free_context(Signal_context_capability cap) override {
			_signal_broker.free_context(cap); }

		void submit(Signal_context_capability cap, unsigned n) override {
			_signal_broker.submit(cap, n); }


		/*****************************
		 ** OKL4-specific additions **
		 *****************************/

		void space_pager(Thread_capability thread);

		Okl4::L4_SpaceId_t space_id() {
			return Okl4::L4_SpaceId(_pd.pd_id()); }
};

#endif /* _CORE__INCLUDE__OKL4__PD_SESSION_COMPONENT_H_ */
