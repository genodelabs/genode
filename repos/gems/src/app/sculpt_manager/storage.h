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

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <os/reporter.h>

/* local includes */
#include <model/discovery_state.h>
#include <view/storage_dialog.h>
#include <view/ram_fs_dialog.h>
#include <runtime.h>

namespace Sculpt { struct Storage; }


struct Sculpt::Storage : Storage_dialog::Action, Ram_fs_dialog::Action
{
	Env &_env;

	Allocator &_alloc;

	Dialog::Generator &_dialog_generator;

	Runtime_config_generator &_runtime_config_generator;

	struct Target_user : Interface
	{
		virtual void use_storage_target(Storage_target const &) = 0;
	};

	Target_user &_target_user;

	Attached_rom_dataspace _block_devices_rom { _env, "report -> drivers/block_devices" };

	Attached_rom_dataspace _usb_active_config_rom { _env, "report -> drivers/usb_active_config" };

	Storage_devices _storage_devices { };

	Ram_fs_state _ram_fs_state;

	Storage_target _sculpt_partition { };

	Discovery_state _discovery_state { };

	Inspect_view_version _inspect_view_version { 0 };

	Storage_dialog dialog { _storage_devices, _sculpt_partition };

	void handle_storage_devices_update();

	Signal_handler<Storage> _storage_device_update_handler {
		_env.ep(), *this, &Storage::handle_storage_devices_update };

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

	template <typename FN>
	void _apply_partition(Storage_target const &target, FN const &fn)
	{
		_storage_devices.for_each([&] (Storage_device &device) {

			if (target.device != device.label)
				return;

			device.for_each_partition([&] (Partition &partition) {

				bool const whole_device = !target.partition.valid()
				                       && !partition.number.valid();

				bool const partition_matches = (device.label == target.device)
				                            && (partition.number == target.partition);

				if (whole_device || partition_matches) {
					fn(partition);
					_runtime_config_generator.generate_runtime_config();
				}
			});
		});
	}

	/**
	 * Storage_dialog::Action interface
	 */
	void format(Storage_target const &target) override
	{
		_apply_partition(target, [&] (Partition &partition) {
			partition.format_in_progress = true; });
	}

	void cancel_format(Storage_target const &target) override
	{
		_apply_partition(target, [&] (Partition &partition) {

			if (partition.format_in_progress) {
				partition.file_system.type   = File_system::UNKNOWN;
				partition.format_in_progress = false;
			}
			dialog.reset_operation();
		});
	}

	void expand(Storage_target const &target) override
	{
		_apply_partition(target, [&] (Partition &partition) {
			partition.gpt_expand_in_progress = true; });
	}

	void cancel_expand(Storage_target const &target) override
	{
		_apply_partition(target, [&] (Partition &partition) {

			if (partition.expand_in_progress()) {
				partition.file_system.type       = File_system::UNKNOWN;
				partition.gpt_expand_in_progress = false;
				partition.fs_resize_in_progress  = false;
			}
			dialog.reset_operation();
		});
	}

	void check(Storage_target const &target) override
	{
		_apply_partition(target, [&] (Partition &partition) {
			partition.check_in_progress = true; });
	}

	void toggle_inspect_view(Storage_target const &target) override
	{
		Inspect_view_version const orig_version = _inspect_view_version;

		if (target.ram_fs()) {
			_ram_fs_state.inspected = !_ram_fs_state.inspected;
			_inspect_view_version.value++;
		}

		_apply_partition(target, [&] (Partition &partition) {
			partition.file_system.inspected = !partition.file_system.inspected;
			_inspect_view_version.value++;
		});

		if (orig_version.value == _inspect_view_version.value)
			return;

		_runtime_config_generator.generate_runtime_config();
	}

	void toggle_default_storage_target(Storage_target const &target) override
	{
		_apply_partition(target, [&] (Partition &partition) {
			partition.toggle_default_label(); });
	}

	void use(Storage_target const &target) override
	{
		_target_user.use_storage_target(target);
	}

	void reset_ram_fs() override
	{
		_ram_fs_state.trigger_restart();

		dialog.reset_operation();
		_runtime_config_generator.generate_runtime_config();
	}


	Storage(Env &env, Allocator &alloc,
	        Registry<Child_state>    &child_states,
	        Dialog::Generator        &dialog_generator,
	        Runtime_config_generator &runtime_config_generator,
	        Target_user              &target_user)
	:
		_env(env), _alloc(alloc),
		_dialog_generator(dialog_generator),
		_runtime_config_generator(runtime_config_generator),
		_target_user(target_user),
		_ram_fs_state(child_states, "ram_fs")
	{
		_block_devices_rom    .sigh(_storage_device_update_handler);
		_usb_active_config_rom.sigh(_storage_device_update_handler);
	}
};

#endif /* _STORAGE_H_ */
