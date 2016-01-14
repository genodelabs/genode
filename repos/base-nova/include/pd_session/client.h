/*
 * \brief  Client-side pd session interface
 * \author Christian Helmuth
 * \date   2006-07-12
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__PD_SESSION__CLIENT_H_
#define _INCLUDE__PD_SESSION__CLIENT_H_

#include <pd_session/capability.h>
#include <base/rpc_client.h>

namespace Genode { struct Pd_session_client; }


/*
 * This implementation overrides the corresponding header in base/include
 * to tweak the way the parent capability is passed to core.
 */
struct Genode::Pd_session_client : Rpc_client<Pd_session>
{
	explicit Pd_session_client(Pd_session_capability session)
	: Rpc_client<Pd_session>(session) { }

	int bind_thread(Thread_capability thread) override {
		return call<Rpc_bind_thread>(thread); }

	int assign_parent(Capability<Parent> parent) override
	{
		/*
		 * NOVA-specific implementation
		 *
		 * We need to prevent NOVA from creating a new branch in the mapping
		 * tree. Instead, we need core to re-associate the supplied PD cap with
		 * the core-known PD session component of the parent.
		 */
		parent.solely_map();
		return call<Rpc_assign_parent>(parent);
	}

	bool assign_pci(addr_t pci_config_memory_address, uint16_t bdf) override {
		return call<Rpc_assign_pci>(pci_config_memory_address, bdf); }

	Signal_source_capability alloc_signal_source() override {
		return call<Rpc_alloc_signal_source>(); }

	void free_signal_source(Signal_source_capability cap) override {
		call<Rpc_free_signal_source>(cap); }

	Signal_context_capability alloc_context(Signal_source_capability source,
	                                        unsigned long imprint) override {
		return call<Rpc_alloc_context>(source, imprint); }

	void free_context(Signal_context_capability cap) override {
		call<Rpc_free_context>(cap); }

	void submit(Signal_context_capability receiver, unsigned cnt = 1) override {
		call<Rpc_submit>(receiver, cnt); }
};

#endif /* _INCLUDE__PD_SESSION__CLIENT_H_ */
