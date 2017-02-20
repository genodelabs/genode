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

#ifndef _INCLUDE__SPEC__X86__PLATFORM_DEVICE__CLIENT_H_
#define _INCLUDE__SPEC__X86__PLATFORM_DEVICE__CLIENT_H_

#include <platform_session/platform_session.h>
#include <platform_device/platform_device.h>
#include <base/rpc_client.h>
#include <io_mem_session/io_mem_session.h>

namespace Platform { struct Device_client; }


struct Platform::Device_client : public Genode::Rpc_client<Device>
{
	Device_client(Device_capability device)
	: Genode::Rpc_client<Device>(device) { }

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

	Genode::Irq_session_capability irq(Genode::uint8_t id) override {
		return call<Rpc_irq>(id); }

	Genode::Io_port_session_capability io_port(Genode::uint8_t id) override {
		return call<Rpc_io_port>(id); }

	Genode::Io_mem_session_capability io_mem(Genode::uint8_t id,
	                                         Genode::Cache_attribute caching = Genode::Cache_attribute::UNCACHED,
	                                         Genode::addr_t offset = 0,
	                                         Genode::size_t size = ~0UL) override {
		return call<Rpc_io_mem>(id, caching, offset, size); }
};

#endif /* _INCLUDE__SPEC__X86__PLATFORM_DEVICE__CLIENT_H_ */
