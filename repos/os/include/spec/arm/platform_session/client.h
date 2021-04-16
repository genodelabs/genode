/*
 * \brief  Client-side Platform-session interface
 * \author Stefan Kalkowski
 * \date   2020-04-28
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SPEC__ARM__PLATFORM_SESSION__CLIENT_H_
#define _INCLUDE__SPEC__ARM__PLATFORM_SESSION__CLIENT_H_

#include <base/rpc_client.h>
#include <platform_session/capability.h>

namespace Platform { struct Client; }


struct Platform::Client : Genode::Rpc_client<Session>
{
	Client(Session_capability session)
	: Rpc_client<Session>(session) { }

	Rom_session_capability devices_rom() override {
		return call<Rpc_devices_rom>(); }

	Capability<Device_interface> acquire_device(Device_name const &name) override {
		return call<Rpc_acquire_device>(name); }

	Capability<Device_interface> acquire_single_device() override {
		return call<Rpc_acquire_single_device>(); }

	void release_device(Capability<Device_interface> device) override {
		call<Rpc_release_device>(device); }

	Ram_dataspace_capability alloc_dma_buffer(size_t size, Cache cache) override {
		return call<Rpc_alloc_dma_buffer>(size, cache); }

	void free_dma_buffer(Ram_dataspace_capability cap) override {
		call<Rpc_free_dma_buffer>(cap); }

	addr_t dma_addr(Ram_dataspace_capability cap) override {
		return call<Rpc_dma_addr>(cap); }
};

#endif /* _INCLUDE__SPEC__ARM__PLATFORM_SESSION__CLIENT_H_ */
