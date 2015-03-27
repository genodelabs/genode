/*
 * \brief  Client-side interface for PCI device
 * \author Norman Feske
 * \date   2008-01-28
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__PCI_DEVICE__CLIENT_H_
#define _INCLUDE__PCI_DEVICE__CLIENT_H_

#include <pci_session/pci_session.h>
#include <pci_device/pci_device.h>
#include <base/rpc_client.h>
#include <io_mem_session/io_mem_session.h>

namespace Pci { struct Device_client; }


struct Pci::Device_client : public Genode::Rpc_client<Device>
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
};

#endif /* _INCLUDE__PCI_DEVICE__CLIENT_H_ */
