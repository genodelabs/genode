/*
 * \brief  Core-specific instance of the PD session interface
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

#ifndef _CORE__INCLUDE__PD_SESSION_COMPONENT_H_
#define _CORE__INCLUDE__PD_SESSION_COMPONENT_H_

/* Genode includes */
#include <util/arg_string.h>
#include <foc_pd_session/foc_pd_session.h>
#include <base/rpc_server.h>

/* core includes */
#include <platform_pd.h>
#include <signal_broker.h>

namespace Genode { class Pd_session_component; }


class Genode::Pd_session_component : public Rpc_object<Foc_pd_session>
{
	private:

		Allocator_guard    _md_alloc;   /* guarded meta-data allocator */
		Platform_pd        _pd;
		Capability<Parent> _parent;
		Rpc_entrypoint    &_thread_ep;
		Signal_broker      _signal_broker;

		size_t _ram_quota(char const * args) {
			return Arg_string::find_arg(args, "ram_quota").long_value(0); }

	public:

		Pd_session_component(Rpc_entrypoint &thread_ep,
		                     Rpc_entrypoint &receiver_ep,
		                     Rpc_entrypoint &context_ep,
		                     Allocator      &md_alloc,
		                     char const     *args)
		:
			_md_alloc(&md_alloc, _ram_quota(args)),
			_thread_ep(thread_ep),
			_signal_broker(_md_alloc, receiver_ep, context_ep)
		{ }

		/**
		 * Register quota donation at allocator guard
		 */
		void upgrade_ram_quota(size_t ram_quota) {
			_md_alloc.upgrade(ram_quota); }


		/**************************
		 ** PD session interface **
		 **************************/

		int bind_thread(Thread_capability) override;

		int assign_parent(Capability<Parent>) override;

		bool assign_pci(addr_t, uint16_t) override
		{
			PWRN("not implemented"); return false;
		};

		Signal_source_capability alloc_signal_source() override {
			return _signal_broker.alloc_signal_source(); }

		void free_signal_source(Signal_source_capability sig_rec_cap) override {
			_signal_broker.free_signal_source(sig_rec_cap); }

		Signal_context_capability
		alloc_context(Signal_source_capability sig_rec_cap, unsigned long imprint) override
		{
			try {
				return _signal_broker.alloc_context(sig_rec_cap, imprint); }
			catch (Genode::Allocator::Out_of_memory) {
				throw Pd_session::Out_of_metadata(); }
		}

		void free_context(Signal_context_capability cap) override {
			_signal_broker.free_context(cap); }

		void submit(Signal_context_capability cap, unsigned n) override {
			_signal_broker.submit(cap, n); }


		/**********************************
		 ** Fiasco.OC specific functions **
		 **********************************/

		Native_capability task_cap();
};

#endif /* _CORE__INCLUDE__PD_SESSION_COMPONENT_H_ */
