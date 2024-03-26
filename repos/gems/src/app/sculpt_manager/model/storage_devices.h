/*
 * \brief  Registry of known storage devices
 * \author Norman Feske
 * \date   2018-05-03
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MODEL__STORAGE_DEVICES_
#define _MODEL__STORAGE_DEVICES_

#include <types.h>
#include <model/ahci_device.h>
#include <model/nvme_device.h>
#include <model/mmc_device.h>
#include <model/usb_storage_device.h>

namespace Sculpt { struct Storage_devices; }


struct Sculpt::Storage_devices : Noncopyable
{
	Storage_device::Action &_action;

	Storage_devices(Storage_device::Action &action) : _action(action) { }

	Ahci_devices        ahci_devices        { };
	Nvme_devices        nvme_devices        { };
	Mmc_devices         mmc_devices         { };
	Usb_storage_devices usb_storage_devices { };

	struct Discovered { bool ahci, nvme, mmc, usb; } _discovered { };

	unsigned num_usb_devices = 0;

	Progress update_ahci(Env &env, Allocator &alloc, Xml_node const &node)
	{
		_discovered.ahci |= (!node.has_type("empty"));

		bool progress = false;
		ahci_devices.update_from_xml(node,

			/* create */
			[&] (Xml_node const &node) -> Ahci_device & {
				progress = true;
				return *new (alloc) Ahci_device(env, alloc, node, _action);
			},

			/* destroy */
			[&] (Ahci_device &a) {
				destroy(alloc, &a);
				progress = true;
			},

			/* update */
			[&] (Ahci_device &, Xml_node const &) { }
		);
		return { progress };
	}

	Progress update_nvme(Env &env, Allocator &alloc, Xml_node const &node)
	{
		_discovered.nvme |= !node.has_type("empty");

		auto const model = node.attribute_value("model", Nvme_device::Model());

		bool progress = false;
		nvme_devices.update_from_xml(node,

			/* create */
			[&] (Xml_node const &node) -> Nvme_device & {
				progress = true;
				return *new (alloc) Nvme_device(env, alloc, model, node, _action);
			},

			/* destroy */
			[&] (Nvme_device &a) {
				destroy(alloc, &a);
				progress = true;
			},

			/* update */
			[&] (Nvme_device &, Xml_node const &) { }
		);
		return { progress };
	}

	Progress update_mmc(Env &env, Allocator &alloc, Xml_node const &node)
	{
		_discovered.mmc |= !node.has_type("empty");

		bool progress = false;
		mmc_devices.update_from_xml(node,

			/* create */
			[&] (Xml_node const &node) -> Mmc_device & {
				progress = true;
				return *new (alloc) Mmc_device(env, alloc, node, _action);
			},

			/* destroy */
			[&] (Mmc_device &d) {
				destroy(alloc, &d);
				progress = true;
			},

			/* update */
			[&] (Mmc_device &, Xml_node const &) { }
		);
		return { progress };
	}

	/**
	 * Update 'usb_storage_devices' from USB devices report
	 *
	 * \return true if USB storage device was added or vanished
	 */
	Progress update_usb(Env &env, Allocator &alloc, Xml_node const &node)
	{
		_discovered.usb |= !node.has_type("empty");

		bool progress = false;
		usb_storage_devices.update_from_xml(node,

			/* create */
			[&] (Xml_node const &node) -> Usb_storage_device &
			{
				progress = true;
				return *new (alloc) Usb_storage_device(env, alloc, node, _action);
			},

			/* destroy */
			[&] (Usb_storage_device &elem)
			{
				progress = true;
				destroy(alloc, &elem);
			},

			/* update */
			[&] (Usb_storage_device &, Xml_node const &) { }
		);

		num_usb_devices = 0;
		usb_storage_devices.for_each([&] (Storage_device const &) {
			num_usb_devices++; });

		return { progress };
	}

	void gen_usb_storage_policies(Xml_generator &xml) const
	{
		usb_storage_devices.for_each([&] (Usb_storage_device const &device) {
			device.gen_usb_policy(xml); });
	}

	void for_each(auto const &fn) const
	{
		ahci_devices       .for_each([&] (Storage_device const &dev) { fn(dev); });
		nvme_devices       .for_each([&] (Storage_device const &dev) { fn(dev); });
		mmc_devices        .for_each([&] (Storage_device const &dev) { fn(dev); });
		usb_storage_devices.for_each([&] (Storage_device const &dev) { fn(dev); });
	}

	void for_each(auto const &fn)
	{
		ahci_devices       .for_each([&] (Storage_device &dev) { fn(dev); });
		nvme_devices       .for_each([&] (Storage_device &dev) { fn(dev); });
		mmc_devices        .for_each([&] (Storage_device &dev) { fn(dev); });
		usb_storage_devices.for_each([&] (Storage_device &dev) { fn(dev); });
	}
};

#endif /* _MODEL__STORAGE_DEVICES_ */
