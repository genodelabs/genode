/*
 * \brief  Client-side interface for PCI device
 * \author Norman Feske
 * \date   2008-01-28
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__LEGACY__X86__PLATFORM_DEVICE__CLIENT_H_
#define _INCLUDE__LEGACY__X86__PLATFORM_DEVICE__CLIENT_H_

#include <legacy/x86/platform_session/platform_session.h>
#include <legacy/x86/platform_device/platform_device.h>
#include <base/rpc_client.h>
#include <io_mem_session/io_mem_session.h>

namespace Platform { struct Device_client; }


struct Platform::Device_client : public Rpc_client<Device>
{
	Device_client(Device_capability device)
	: Rpc_client<Device>(device) { }

	void bus_address(unsigned char *bus, unsigned char *dev, unsigned char *fn) override {
		call<Rpc_bus_address>(bus, dev, fn); }

	unsigned short vendor_id() override {
		return call<Rpc_vendor_id>(); }

	unsigned short device_id() override {
		return call<Rpc_device_id>(); }

	unsigned class_code() override {
		return call<Rpc_class_code>(); }

	Resource resource(int resource_id) override {
		return call<Rpc_resource>(resource_id); }

	unsigned config_read(unsigned char address, Access_size size) override {
		return call<Rpc_config_read>(address, size); }

	void config_write(unsigned char address, unsigned value, Access_size size) override {
		call<Rpc_config_write>(address, value, size); }

	Irq_session_capability irq(uint8_t id) override {
		return call<Rpc_irq>(id); }

	Io_port_session_capability io_port(uint8_t id) override {
		return call<Rpc_io_port>(id); }

	Io_mem_session_capability io_mem(uint8_t id,
	                                 Cache   cache  = Cache::UNCACHED,
	                                 addr_t  offset = 0,
	                                 size_t  size   = ~0UL) override {
		return call<Rpc_io_mem>(id, cache, offset, size); }
};

#endif /* _INCLUDE__LEGACY__X86__PLATFORM_DEVICE__CLIENT_H_ */
