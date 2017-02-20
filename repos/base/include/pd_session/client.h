/*
 * \brief  Client-side pd session interface
 * \author Christian Helmuth
 * \date   2006-07-12
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__PD_SESSION__CLIENT_H_
#define _INCLUDE__PD_SESSION__CLIENT_H_

#include <pd_session/capability.h>
#include <base/rpc_client.h>

namespace Genode { struct Pd_session_client; }


struct Genode::Pd_session_client : Rpc_client<Pd_session>
{
	explicit Pd_session_client(Pd_session_capability session)
	: Rpc_client<Pd_session>(session) { }

	void assign_parent(Capability<Parent> parent) override {
		call<Rpc_assign_parent>(parent); }

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

	Native_capability alloc_rpc_cap(Native_capability ep) override {
		return call<Rpc_alloc_rpc_cap>(ep); }

	void free_rpc_cap(Native_capability cap) override {
		call<Rpc_free_rpc_cap>(cap); }

	Capability<Region_map> address_space() override {
		return call<Rpc_address_space>(); }

	Capability<Region_map> stack_area() override {
		return call<Rpc_stack_area>(); }

	Capability<Region_map> linker_area() override {
		return call<Rpc_linker_area>(); }

	Capability<Native_pd> native_pd() override { return call<Rpc_native_pd>(); }
};

#endif /* _INCLUDE__PD_SESSION__CLIENT_H_ */
