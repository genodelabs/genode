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


Progress Storage::update(Xml_node const &config,
                         Xml_node const &usb,  Xml_node const &ahci,
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

	Storage_target const orig_selected_target   = _selected_target;
	Storage_target const orig_configured_target = _configured_target;

	config.with_sub_node("target",
		[&] (Xml_node const &target) {
			Storage_target const configured_target = Storage_target::from_xml(target);
			if (configured_target != _configured_target) {
				_configured_target = configured_target;
				_malconfiguration = false; } },

		[&] { _configured_target = { }; });

	if (orig_configured_target != _configured_target)
		_selected_target = { };

	if (!_selected_target.valid()) {

		bool const all_devices_enumerated = !usb .has_type("empty")
		                                 && !ahci.has_type("empty")
		                                 && !nvme.has_type("empty")
		                                 && !mmc .has_type("empty");
		if (all_devices_enumerated) {
			if (_configured_target.valid())
				_selected_target = _malconfiguration ? Storage_target { } : _configured_target;
			else
				_selected_target = _discovery_state.detect_default_target(_storage_devices);
		}
	}

	/*
	 * Detect the removal of a USB stick that is currently in "use". Reset
	 * the '_selected_target' to enable the selection of another storage
	 * target to use.
	 */
	else if (_selected_target.valid()) {

		bool selected_target_exists = false;

		if (_selected_target.ram_fs())
			selected_target_exists = true;

		_storage_devices.for_each([&] (Storage_device const &device) {
			device.for_each_partition([&] (Partition const &partition) {
				if (device.driver    == _selected_target.driver
				 && partition.number == _selected_target.partition)
					selected_target_exists = true; }); });

		if (!selected_target_exists) {
			if (_configured_target.valid()) {
				warning("configured storage target does not exist");
				_malconfiguration = true;
			} else {
				warning("selected storage target unexpectedly vanished");
			}

			_selected_target   = { };
		}
	}

	progress |= (orig_selected_target != _selected_target);

	return { progress };
}


void Storage::gen_runtime_start_nodes(Xml_generator &xml) const
{
	xml.node("start", [&] {
		gen_ram_fs_start_content(xml, _ram_fs_state); });

	auto contains_used_fs = [&] (Storage_device const &device)
	{
		if (!_selected_target.valid())
			return false;

		return (device.port   == _selected_target.port)
		    && (device.driver == _selected_target.driver);
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
				if (partition.file_system.inspected || target == _selected_target)
					xml.node("start", [&] {
						gen_fs_start_content(xml, target, partition.file_system.type); });

				/*
				 * Create alias so that the default file system can be referred
				 * to as "default_fs_rw" without the need to know the name of the
				 * underlying storage target.
				 */
				if (target == _selected_target)
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

	if (_selected_target.ram_fs())
		gen_named_node(xml, "alias", "default_fs_rw", [&] {
			xml.attribute("child", "ram_fs"); });
}

