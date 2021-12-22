/*
 * \brief  Client-side PCI-session interface
 * \author Norman Feske
 * \date   2008-01-28
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__LEGACY__X86__PLATFORM_SESSION_H_
#define _INCLUDE__LEGACY__X86__PLATFORM_SESSION_H_

#include <base/rpc_client.h>
#include <legacy/x86/platform_device/client.h>
#include <legacy/x86/platform_session/capability.h>

namespace Platform { struct Client; }


struct Platform::Client : public Rpc_client<Session>
{
	Client(Session_capability session) : Rpc_client<Session>(session) { }

	Device_capability first_device(unsigned device_class = 0,
	                               unsigned class_mask = 0) override {
		return call<Rpc_first_device>(device_class, class_mask); }

	Device_capability next_device(Device_capability prev_device,
	                              unsigned device_class = 0,
	                              unsigned class_mask = 0) override {
		return call<Rpc_next_device>(prev_device, device_class, class_mask); }

	void release_device(Device_capability device) override {
		call<Rpc_release_device>(device); }

	Ram_dataspace_capability alloc_dma_buffer(size_t size, Cache cache) override {
		return call<Rpc_alloc_dma_buffer>(size, cache); }

	void free_dma_buffer(Ram_dataspace_capability cap) override {
		call<Rpc_free_dma_buffer>(cap); }

	addr_t dma_addr(Ram_dataspace_capability cap) override {
		return call<Rpc_dma_addr>(cap); }

	Device_capability device(Device_name const &device) override {
		return call<Rpc_device>(device); }
};

#endif /* _INCLUDE__LEGACY__X86__PLATFORM_SESSION_H_ */
