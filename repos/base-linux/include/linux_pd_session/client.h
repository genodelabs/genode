/*
 * \brief  Client-side PD session interface
 * \author Norman Feske
 * \date   2012-08-15
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__LINUX_PD_SESSION__CLIENT_H_
#define _INCLUDE__LINUX_PD_SESSION__CLIENT_H_

#include <linux_pd_session/linux_pd_session.h>
#include <base/rpc_client.h>

namespace Genode { struct Linux_pd_session_client; }

struct Genode::Linux_pd_session_client : Rpc_client<Linux_pd_session>
{
	explicit Linux_pd_session_client(Capability<Linux_pd_session> session)
	: Rpc_client<Linux_pd_session>(session) { }

	int bind_thread(Thread_capability thread) override {
		return call<Rpc_bind_thread>(thread); }

	int assign_parent(Capability<Parent> parent) override {
		return call<Rpc_assign_parent>(parent); }

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


	/*****************************
	 * Linux-specific extension **
	 *****************************/

	void start(Capability<Dataspace> binary) {
		call<Rpc_start>(binary); }
};

#endif /* _INCLUDE__LINUX_PD_SESSION__CLIENT_H_ */
