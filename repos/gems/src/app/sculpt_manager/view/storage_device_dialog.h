/*
 * \brief  Storage-device management dialog
 * \author Norman Feske
 * \date   2020-01-29
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW__STORAGE_DEVICE_DIALOG_H_
#define _VIEW__STORAGE_DEVICE_DIALOG_H_

#include <types.h>
#include <model/storage_devices.h>
#include <model/storage_target.h>
#include <view/selectable_item.h>
#include <view/activatable_item.h>
#include <view/partition_dialog.h>

namespace Sculpt { struct Storage_device_dialog; }


struct Sculpt::Storage_device_dialog : Dialog
{
	Storage_device::Label const  _device;
	Storage_devices       const &_storage_devices;
	Storage_target        const &_used_target;

	Selectable_item _partition_item { };

	Reconstructible<Partition_dialog> _partition_dialog {
		_selected_storage_target(), _storage_devices, _used_target };

	void generate(Xml_generator &) const override { }

	void _gen_partition(Xml_generator &, Storage_device const &, Partition const &) const;

	void generate(Xml_generator &, Storage_device const &) const;

	Hover_result hover(Xml_node hover) override;

	using Action = Partition_dialog::Action;

	Storage_target _selected_storage_target() const
	{
		Partition::Number partition = (_partition_item._selected == "")
		                            ? Partition::Number { }
		                            : Partition::Number(_partition_item._selected);

		return Storage_target { _device, partition };
	}

	void reset() override { }

	void reset_operation()
	{
		if (_partition_dialog.constructed())
			_partition_dialog->reset_operation();
	}

	Click_result click(Action &action);
	Clack_result clack(Action &action);

	Storage_device_dialog(Storage_device::Label const &device,
	                      Storage_devices       const &storage_devices,
	                      Storage_target        const &used)
	:
		_device(device),
		_storage_devices(storage_devices),
		_used_target(used)
	{ }
};

#endif /* _VIEW__STORAGE_DEVICE_DIALOG_H_ */
