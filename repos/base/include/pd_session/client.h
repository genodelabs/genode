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
#include <dataspace/client.h>

namespace Genode { struct Pd_session_client; }


struct Genode::Pd_session_client : Rpc_client<Pd_session>
{
	explicit Pd_session_client(Pd_session_capability session)
	: Rpc_client<Pd_session>(session) { }

	void assign_parent(Capability<Parent> parent) override {
		call<Rpc_assign_parent>(parent); }

	bool assign_pci(addr_t pci_config_memory_address, uint16_t bdf) override {
		return call<Rpc_assign_pci>(pci_config_memory_address, bdf); }

	void map(addr_t virt, addr_t size) override { call<Rpc_map>(virt, size); }

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

	void ref_account(Capability<Pd_session> pd) override {
		call<Rpc_ref_account>(pd); }

	void transfer_quota(Capability<Pd_session> pd, Cap_quota amount) override {
		call<Rpc_transfer_cap_quota>(pd, amount); }

	Cap_quota cap_quota() const override { return call<Rpc_cap_quota>(); }
	Cap_quota used_caps() const override { return call<Rpc_used_caps>(); }

	Alloc_result try_alloc(size_t size, Cache cache = CACHED) override
	{
		return call<Rpc_try_alloc>(size, cache);
	}

	void free(Ram_dataspace_capability ds) override { call<Rpc_free>(ds); }

	size_t dataspace_size(Ram_dataspace_capability ds) const override
	{
		return ds.valid() ? Dataspace_client(ds).size() : 0;
	}

	void transfer_quota(Pd_session_capability pd_session, Ram_quota amount) override {
		call<Rpc_transfer_ram_quota>(pd_session, amount); }

	Ram_quota ram_quota() const override { return call<Rpc_ram_quota>(); }
	Ram_quota used_ram()  const override { return call<Rpc_used_ram>(); }

	Capability<Native_pd> native_pd() override { return call<Rpc_native_pd>(); }

	Managing_system_state managing_system(Managing_system_state const & state) override {
		return call<Rpc_managing_system>(state); }

	addr_t dma_addr(Ram_dataspace_capability ds) override { return call<Rpc_dma_addr>(ds); }

	Attach_dma_result attach_dma(Dataspace_capability ds, addr_t at) override {
		return call<Rpc_attach_dma>(ds, at); }
};

#endif /* _INCLUDE__PD_SESSION__CLIENT_H_ */
