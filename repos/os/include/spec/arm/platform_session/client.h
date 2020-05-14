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
#include <platform_device/client.h>
#include <platform_session/capability.h>

namespace Platform {
	using namespace Genode;

	struct Client;
}


struct Platform::Client : public Genode::Rpc_client<Session>
{
	Client(Session_capability session)
	: Rpc_client<Session>(session) { }

	Rom_session_capability devices_rom() override {
		return call<Rpc_devices_rom>(); }

	Device_capability acquire_device(String const &device) override {
		return call<Rpc_acquire_device>(device); }

	void release_device(Device_capability device) override {
		call<Rpc_release_device>(device); }

	Ram_dataspace_capability alloc_dma_buffer(size_t size) override {
		return call<Rpc_alloc_dma_buffer>(size); }

	void free_dma_buffer(Ram_dataspace_capability cap) override {
		call<Rpc_free_dma_buffer>(cap); }

	addr_t bus_addr_dma_buffer(Ram_dataspace_capability cap) override {
		return call<Rpc_bus_addr_dma_buffer>(cap); }
};

#endif /* _INCLUDE__SPEC__ARM__PLATFORM_SESSION__CLIENT_H_ */
