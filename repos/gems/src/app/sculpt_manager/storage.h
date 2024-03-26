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

#ifndef _STORAGE_H_
#define _STORAGE_H_

/* local includes */
#include <model/discovery_state.h>
#include <runtime.h>

namespace Sculpt { struct Storage; }


struct Sculpt::Storage : Noncopyable
{
	Env &_env;

	Allocator &_alloc;

	Storage_devices _storage_devices;

	Ram_fs_state _ram_fs_state;

	Storage_target _sculpt_partition { };

	Discovery_state _discovery_state { };

	Inspect_view_version _inspect_view_version { 0 };

	Progress update(Xml_node const &usb_devices,     Xml_node const &ahci_ports,
	                Xml_node const &nvme_namespaces, Xml_node const &mmc_devices);

	/*
	 * Determine whether showing the file-system browser or not
	 */
	bool any_file_system_inspected() const
	{
		bool result = _ram_fs_state.inspected;
		_storage_devices.for_each([&] (Storage_device const &device) {
			device.for_each_partition([&] (Partition const &partition) {
				result |= partition.file_system.inspected; }); });
		return result;
	}

	void gen_runtime_start_nodes(Xml_generator &) const;

	void gen_usb_storage_policies(Xml_generator &xml) const
	{
		_storage_devices.gen_usb_storage_policies(xml);
	}

	void _apply_partition(Storage_target const &target, auto const &fn)
	{
		_storage_devices.for_each([&] (Storage_device &device) {

			if (target.driver != device.driver)
				return;

			device.for_each_partition([&] (Partition &partition) {

				bool const whole_device = !target.partition.valid()
				                       && !partition.number.valid();

				bool const partition_matches = (device.driver    == target.driver)
				                            && (partition.number == target.partition);

				if (whole_device || partition_matches)
					fn(partition);
			});
		});
	}

	void format(Storage_target const &target)
	{
		_apply_partition(target, [&] (Partition &partition) {
			partition.format_in_progress = true; });
	}

	void cancel_format(Storage_target const &target)
	{
		_apply_partition(target, [&] (Partition &partition) {

			if (partition.format_in_progress) {
				partition.file_system.type   = File_system::UNKNOWN;
				partition.format_in_progress = false;
			}
		});
	}

	void expand(Storage_target const &target)
	{
		_apply_partition(target, [&] (Partition &partition) {
			partition.gpt_expand_in_progress = true; });
	}

	void cancel_expand(Storage_target const &target)
	{
		_apply_partition(target, [&] (Partition &partition) {

			if (partition.expand_in_progress()) {
				partition.file_system.type       = File_system::UNKNOWN;
				partition.gpt_expand_in_progress = false;
				partition.fs_resize_in_progress  = false;
			}
		});
	}

	void check(Storage_target const &target)
	{
		_apply_partition(target, [&] (Partition &partition) {
			partition.check_in_progress = true; });
	}

	void toggle_inspect_view(Storage_target const &target)
	{
		if (target.ram_fs()) {
			_ram_fs_state.inspected = !_ram_fs_state.inspected;
			_inspect_view_version.value++;
		}

		_apply_partition(target, [&] (Partition &partition) {
			partition.file_system.inspected = !partition.file_system.inspected;
			_inspect_view_version.value++;
		});
	}

	void toggle_default_storage_target(Storage_target const &target)
	{
		_apply_partition(target, [&] (Partition &partition) {
			partition.toggle_default_label(); });
	}

	void reset_ram_fs()
	{
		_ram_fs_state.trigger_restart();
	}

	Storage(Env &env, Allocator &alloc, Registry<Child_state> &child_states,
	        Storage_device::Action &action)
	:
		_env(env), _alloc(alloc), _storage_devices(action),
		_ram_fs_state(child_states, "ram_fs")
	{ }
};

#endif /* _STORAGE_H_ */
