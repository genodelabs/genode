/*
 * \brief  Core-specific pseudo PD session
 * \author Norman Feske
 * \date   2016-01-13
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__CORE_PD_SESSION_H_
#define _CORE__INCLUDE__CORE_PD_SESSION_H_

/* Genode includes */
#include <base/rpc_server.h>
#include <base/allocator.h>
#include <pd_session/pd_session.h>

/* core includes */
#include <assertion.h>
#include <signal_source_component.h>

namespace Genode { class Core_pd_session_component; }


class Genode::Core_pd_session_component : public Rpc_object<Pd_session>
{
	private:

		Rpc_entrypoint &_signal_source_ep;

	public:

		/**
		 * Constructor
		 *
		 * \param context_ep  entrypoint that serves the signal-source
		 *                    components
		 */
		Core_pd_session_component(Rpc_entrypoint &signal_source_ep)
		:
			_signal_source_ep(signal_source_ep)
		{ }

		void assign_parent(Capability<Parent> parent) override
		{
			ASSERT_NEVER_CALLED;
		}

		bool assign_pci(addr_t pci_config_memory_address, uint16_t) override
		{
			ASSERT_NEVER_CALLED;
		}

		Signal_source_capability alloc_signal_source() override
		{
			/*
			 * Even though core does not receive any signals, this function is
			 * called by the base-common initialization code on base-hw. We
			 * can savely return an invalid capability as it is never used.
			 */
			return Signal_source_capability();
		}

		void free_signal_source(Signal_source_capability cap) override
		{
			ASSERT_NEVER_CALLED;
		}

		Capability<Signal_context>
		alloc_context(Signal_source_capability source, unsigned long imprint) override
		{
			ASSERT_NEVER_CALLED;
		}

		void free_context(Capability<Signal_context> cap) override
		{
			ASSERT_NEVER_CALLED;
		}

		void submit(Capability<Signal_context> cap, unsigned cnt = 1) override
		{
			_signal_source_ep.apply(cap, [&] (Signal_context_component *context) {
				if (!context) {
					warning("invalid signal-context capability");
					return;
				}

				context->source()->submit(context, cnt);
			});
		}

		Native_capability alloc_rpc_cap(Native_capability) override
		{
			ASSERT_NEVER_CALLED;
		}

		void free_rpc_cap(Native_capability) override
		{
			ASSERT_NEVER_CALLED;
		}

		Capability<Region_map> address_space() { ASSERT_NEVER_CALLED; }

		Capability<Region_map> stack_area() { ASSERT_NEVER_CALLED; }

		Capability<Region_map> linker_area() { ASSERT_NEVER_CALLED; }

		Capability<Native_pd> native_pd() override { ASSERT_NEVER_CALLED; }
};

#endif /* _CORE__INCLUDE__CORE_PD_SESSION_H_ */
