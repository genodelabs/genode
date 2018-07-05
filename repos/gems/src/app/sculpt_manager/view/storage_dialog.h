/*
 * \brief  Storage management dialog
 * \author Norman Feske
 * \date   2018-04-30
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW__STORAGE_DIALOG_H_
#define _VIEW__STORAGE_DIALOG_H_

#include <types.h>
#include <model/storage_devices.h>
#include <model/storage_target.h>
#include <model/ram_fs_state.h>
#include <view/selectable_item.h>
#include <view/activatable_item.h>
#include <view/dialog.h>

namespace Sculpt { struct Storage_dialog; }


struct Sculpt::Storage_dialog : Dialog
{
	Env &_env;

	Dialog::Generator &_dialog_generator;

	Storage_devices const &_storage_devices;

	Ram_fs_state const &_ram_fs_state;

	Selectable_item  _device_item    { };
	Selectable_item  _partition_item { };
	Hoverable_item   _use_item       { };
	Hoverable_item   _relabel_item   { };
	Hoverable_item   _inspect_item   { };
	Selectable_item  _operation_item { };
	Activatable_item _confirm_item   { };

	void generate(Xml_generator &, bool expanded) const;

	void _gen_block_device(Xml_generator &, Block_device const &) const;

	void _gen_partition(Xml_generator &, Storage_device const &, Partition const &) const;

	void _gen_partition_operations(Xml_generator &, Storage_device const &, Partition const &) const;

	void _gen_usb_storage_device(Xml_generator &, Usb_storage_device const &) const;

	void _gen_ram_fs(Xml_generator &) const;

	void hover(Xml_node hover);

	Storage_target const &_used_target;

	struct Action : Interface
	{
		virtual void format(Storage_target const &) = 0;

		virtual void cancel_format(Storage_target const &) = 0;

		virtual void expand(Storage_target const &) = 0;

		virtual void cancel_expand(Storage_target const &) = 0;

		virtual void check(Storage_target const &) = 0;

		virtual void toggle_file_browser(Storage_target const &) = 0;

		virtual void toggle_default_storage_target(Storage_target const &) = 0;

		virtual void use(Storage_target const &) = 0;

		virtual void reset_ram_fs() = 0;
	};

	Storage_target _selected_storage_target() const
	{
		Partition::Number partition = (_partition_item._selected == "")
		                            ? Partition::Number { }
		                            : Partition::Number(_partition_item._selected);

		return Storage_target { _device_item._selected, partition };
	}

	void reset_operation()
	{
		_operation_item.reset();
		_confirm_item.reset();

		_dialog_generator.generate_dialog();
	}

	void click(Action &action);

	void clack(Action &action);

	Storage_dialog(Env &env, Dialog::Generator &dialog_generator,
	               Storage_devices const &storage_devices,
	               Ram_fs_state const &ram_fs_state, Storage_target const &used)
	:
		_env(env), _dialog_generator(dialog_generator),
		_storage_devices(storage_devices), _ram_fs_state(ram_fs_state),
		_used_target(used)
	{ }
};

#endif /* _STORAGE_DIALOG_H_ */
