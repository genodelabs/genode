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

#pragma once

#include <base/rpc_client.h>

#include <platform_device/client.h>
#include <platform_session/capability.h>

namespace Platform { struct Client; }


struct Platform::Client : public Genode::Rpc_client<Session>
{
	Client(Session_capability session)
	: Genode::Rpc_client<Session>(session) { }

	Device_capability first_device(unsigned device_class = 0,
	                               unsigned class_mask = 0) override {
		return call<Rpc_first_device>(device_class, class_mask); }

	Device_capability next_device(Device_capability prev_device,
	                              unsigned device_class = 0,
	                              unsigned class_mask = 0) override {
		return call<Rpc_next_device>(prev_device, device_class, class_mask); }

	void release_device(Device_capability device) override {
		call<Rpc_release_device>(device); }

	Genode::Ram_dataspace_capability alloc_dma_buffer(Genode::size_t size) override {
		return call<Rpc_alloc_dma_buffer>(size); }

	void free_dma_buffer(Genode::Ram_dataspace_capability cap) override {
		call<Rpc_free_dma_buffer>(cap); }

	Device_capability device(String const &device) override {
		return call<Rpc_device>(device); }
};
