/*
 * \brief  State of automated selection of the sculpt partition
 * \author Norman Feske
 * \date   2018-05-16
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MODEL__DISCOVERY_STATE_H_
#define _MODEL__DISCOVERY_STATE_H_

/* Genode includes */
#include <util/xml_node.h>

/* local includes */
#include <types.h>
#include <model/storage_target.h>
#include <model/storage_devices.h>

namespace Sculpt { struct Discovery_state; }

struct Sculpt::Discovery_state
{
	bool _done = false;

	bool discovery_in_progress() const { return !_done; }

	Storage_target detect_default_target(Storage_devices const &devices)
	{
		/* apply the automated selection only once */
		if (_done)
			return Storage_target { };

		/* defer decision until the low-level device enumeration is complete */
		if (!devices.all_devices_enumerated())
			return Storage_target { };

		/*
		 * As long as not all devices are completely known, it is too early to
		 * take a decision.
		 */
		bool all_devices_discovered = true;
		devices.for_each([&] (Storage_device const &device) {
			if (device.state == Storage_device::UNKNOWN)
				all_devices_discovered = false; });

		if (!all_devices_discovered)
			return Storage_target { };

		/*
		 * Search for a partition with the magic label "GENODE*", or - if no
		 * such partition is present - a whole-device file system.
		 *
		 * Prefer USB storage devices over block devices. If no partition with
		 * the magic label is present but a block device that is formatted as
		 * a file system (the Sculpt EA way), this block device is selected.
		 */
		Storage_target target { };
		auto check_partitions = [&] (Storage_device const &device) {

			device.for_each_partition([&] (Partition const &partition) {
				if (!partition.whole_device()
				 && partition.label == "GENODE*"
				 && partition.file_system.accessible())
					target = Storage_target { device.label, device.port, partition.number }; });
		};

		if (!target.valid())
			devices.usb_storage_devices.for_each([&] (Storage_device const &device) {
				check_partitions(device); });

		if (!target.valid())
			devices.block_devices.for_each([&] (Storage_device const &device) {
				check_partitions(device); });

		if (!target.valid())
			devices.block_devices.for_each([&] (Storage_device const &device) {
				if (device.whole_device
				 && device.whole_device_partition->file_system.accessible())
					target = Storage_target { device.label, device.port, Partition::Number() }; });

		if (target.valid())
			_done = true;

		return target;
	}
};

#endif /* _MODEL__DISCOVERY_STATE_H_ */
