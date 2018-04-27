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


void Sculpt::Storage::handle_storage_devices_update()
{
	bool reconfigure_runtime = false;
	{
		_block_devices_rom.update();
		Block_device_update_policy policy(_env, _alloc, _storage_device_update_handler);
		_storage_devices.update_block_devices_from_xml(policy, _block_devices_rom.xml());

		_storage_devices.block_devices.for_each([&] (Block_device &dev) {

			dev.process_part_blk_report();

			if (dev.state == Storage_device::UNKNOWN) {
				reconfigure_runtime = true; };
		});
	}

	{
		_usb_active_config_rom.update();
		Usb_storage_device_update_policy policy(_env, _alloc, _storage_device_update_handler);
		Xml_node const config = _usb_active_config_rom.xml();
		Xml_node const raw = config.has_sub_node("raw")
		                   ? config.sub_node("raw") : Xml_node("<raw/>");

		_storage_devices.update_usb_storage_devices_from_xml(policy, raw);

		_storage_devices.usb_storage_devices.for_each([&] (Usb_storage_device &dev) {

			dev.process_driver_report();
			dev.process_part_blk_report();

			if (dev.state == Storage_device::UNKNOWN) {
				reconfigure_runtime = true; };
		});
	}

	if (!_sculpt_partition.valid()) {

		Storage_target const default_target =
			_discovery_state.detect_default_target(_storage_devices);

		if (default_target.valid())
			use(default_target);
	}

	_dialog_generator.generate_dialog();

	if (reconfigure_runtime)
		_runtime_config_generator.generate_runtime_config();
}


void Sculpt::Storage::gen_runtime_start_nodes(Xml_generator &xml) const
{
	xml.node("start", [&] () {
		gen_ram_fs_start_content(xml, _ram_fs_state); });

	auto part_blk_needed_for_use = [&] (Storage_device const &dev) {
		return (_sculpt_partition.device == dev.label)
		     && _sculpt_partition.partition.valid(); };

	_storage_devices.block_devices.for_each([&] (Block_device const &dev) {
	
		if (dev.part_blk_needed_for_discovery()
		 || dev.part_blk_needed_for_access()
		 || part_blk_needed_for_use(dev))

			xml.node("start", [&] () {
				Storage_device::Label const parent { };
				dev.gen_part_blk_start_content(xml, parent); }); });

	_storage_devices.usb_storage_devices.for_each([&] (Usb_storage_device const &dev) {

		if (dev.usb_block_drv_needed() || _sculpt_partition.device == dev.label)
			xml.node("start", [&] () {
				dev.gen_usb_block_drv_start_content(xml); });

		if (dev.part_blk_needed_for_discovery()
		 || dev.part_blk_needed_for_access()
		 || part_blk_needed_for_use(dev))

			xml.node("start", [&] () {
				Storage_device::Label const driver = dev.usb_block_drv_name();
				dev.gen_part_blk_start_content(xml, driver);
		});
	});

	_storage_devices.for_each([&] (Storage_device const &device) {

		device.for_each_partition([&] (Partition const &partition) {

			Storage_target const target { device.label, partition.number };

			if (partition.check_in_progress) {
				xml.node("start", [&] () {
					gen_fsck_ext2_start_content(xml, target); }); }

			if (partition.format_in_progress) {
				xml.node("start", [&] () {
					gen_mkfs_ext2_start_content(xml, target); }); }

			if (partition.fs_resize_in_progress) {
				xml.node("start", [&] () {
					gen_resize2fs_start_content(xml, target); }); }

			if (partition.file_system.type != File_system::UNKNOWN) {
				if (partition.file_system_inspected || target == _sculpt_partition)
					xml.node("start", [&] () {
						gen_fs_start_content(xml, target, partition.file_system.type); });

				/*
				 * Create alias so that the default file system can be referred
				 * to as "default_fs_rw" without the need to know the name of the
				 * underlying storage target.
				 */
				if (target == _sculpt_partition)
					gen_named_node(xml, "alias", "default_fs_rw", [&] () {
						xml.attribute("child", target.fs()); });
			}

		}); /* for each partition */

		/* relabel partitions if needed */
		if (device.relabel_in_progress())
			xml.node("start", [&] () {
				gen_gpt_relabel_start_content(xml, device); });

		/* expand partitions if needed */
		if (device.expand_in_progress())
			xml.node("start", [&] () {
				gen_gpt_expand_start_content(xml, device); });

	}); /* for each device */

	if (_sculpt_partition.ram_fs())
		gen_named_node(xml, "alias", "default_fs_rw", [&] () {
			xml.attribute("child", "ram_fs"); });
}

