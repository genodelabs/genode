/*
 * \brief  Sculpt storage management
 * \author Norman Feske
 * \date   2018-04-30
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include <storage.h>

using namespace Sculpt;


Progress Storage::update(Xml_node const &usb,  Xml_node const &ahci,
                         Xml_node const &nvme, Xml_node const &mmc)
{
	bool progress = false;

	progress |= _storage_devices.update_ahci(_env, _alloc, ahci).progress;
	progress |= _storage_devices.update_nvme(_env, _alloc, nvme).progress;
	progress |= _storage_devices.update_mmc (_env, _alloc, mmc) .progress;
	progress |= _storage_devices.update_usb (_env, _alloc, usb) .progress;

	_storage_devices.for_each([&] (Storage_device &dev) {
		Storage_device::State const orig_state = dev.state;
		dev.process_partitions();
		progress |= (dev.state != orig_state);
	});

	_storage_devices.usb_storage_devices.for_each([&] (Usb_storage_device &dev) {
		dev.process_report(); });

	if (!_sculpt_partition.valid()) {

		bool const all_devices_enumerated = !usb .has_type("empty")
		                                 && !ahci.has_type("empty")
		                                 && !nvme.has_type("empty")
		                                 && !mmc .has_type("empty");
		if (all_devices_enumerated) {

			Storage_target const default_target =
				_discovery_state.detect_default_target(_storage_devices);

			if (default_target.valid()) {
				_sculpt_partition = default_target;
				progress |= true;
			}
		}
	}

	/*
	 * Detect the removal of a USB stick that is currently in "use". Reset
	 * the '_sculpt_partition' to enable the selection of another storage
	 * target to use.
	 */
	else if (_sculpt_partition.valid()) {

		bool sculpt_partition_exists = false;

		if (_sculpt_partition.ram_fs())
			sculpt_partition_exists = true;

		_storage_devices.for_each([&] (Storage_device const &device) {
			device.for_each_partition([&] (Partition const &partition) {
				if (device.driver    == _sculpt_partition.driver
				 && partition.number == _sculpt_partition.partition)
					sculpt_partition_exists = true; }); });

		if (!sculpt_partition_exists) {
			warning("sculpt partition unexpectedly vanished");
			_sculpt_partition = Storage_target { };
			progress |= true;
		}
	}

	return { progress };
}


void Storage::gen_runtime_start_nodes(Xml_generator &xml) const
{
	xml.node("start", [&] {
		gen_ram_fs_start_content(xml, _ram_fs_state); });

	auto contains_used_fs = [&] (Storage_device const &device)
	{
		if (!_sculpt_partition.valid())
			return false;

		return (device.port   == _sculpt_partition.port)
		    && (device.driver == _sculpt_partition.driver);
	};

	_storage_devices.usb_storage_devices.for_each([&] (Usb_storage_device const &device) {

		if (device.usb_block_drv_needed() || contains_used_fs(device))
			xml.node("start", [&] {
				device.gen_usb_block_drv_start_content(xml); });
	});

	_storage_devices.for_each([&] (Storage_device const &device) {

		bool const device_contains_used_fs_in_partition =
			contains_used_fs(device) && !device.whole_device;

		bool const part_block_needed = device.part_block_needed_for_discovery()
		                            || device.part_block_needed_for_access()
		                            || device_contains_used_fs_in_partition;
		if (part_block_needed)
			xml.node("start", [&] {
				device.gen_part_block_start_content(xml); });

		device.for_each_partition([&] (Partition const &partition) {

			Storage_target const target { .driver    = device.driver,
			                              .port      = device.port,
			                              .partition = partition.number };

			if (partition.check_in_progress) {
				xml.node("start", [&] {
					gen_fsck_ext2_start_content(xml, target); }); }

			if (partition.format_in_progress) {
				xml.node("start", [&] {
					gen_mkfs_ext2_start_content(xml, target); }); }

			if (partition.fs_resize_in_progress) {
				xml.node("start", [&] {
					gen_resize2fs_start_content(xml, target); }); }

			if (partition.file_system.type != File_system::UNKNOWN) {
				if (partition.file_system.inspected || target == _sculpt_partition)
					xml.node("start", [&] {
						gen_fs_start_content(xml, target, partition.file_system.type); });

				/*
				 * Create alias so that the default file system can be referred
				 * to as "default_fs_rw" without the need to know the name of the
				 * underlying storage target.
				 */
				if (target == _sculpt_partition)
					gen_named_node(xml, "alias", "default_fs_rw", [&] {
						xml.attribute("child", target.fs()); });
			}

		}); /* for each partition */

		/* relabel partitions if needed */
		if (device.relabel_in_progress())
			xml.node("start", [&] {
				gen_gpt_relabel_start_content(xml, device); });

		/* expand partitions if needed */
		if (device.expand_in_progress())
			xml.node("start", [&] {
				gen_gpt_expand_start_content(xml, device); });

	}); /* for each device */

	if (_sculpt_partition.ram_fs())
		gen_named_node(xml, "alias", "default_fs_rw", [&] {
			xml.attribute("child", "ram_fs"); });
}

