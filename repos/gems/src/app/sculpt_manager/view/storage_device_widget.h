/*
 * \brief  Storage-device management widget
 * \author Norman Feske
 * \date   2020-01-29
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW__STORAGE_DEVICE_WIDGET_H_
#define _VIEW__STORAGE_DEVICE_WIDGET_H_

#include <types.h>
#include <view/partition_operations.h>

namespace Sculpt { struct Storage_device_widget; }


struct Sculpt::Storage_device_widget : Widget<Vbox>
{
	Partition::Number _selected_partition { };

	Partition_operations _partition_operations { };

	void view(Scope<Vbox> &, Storage_device const &, Storage_target const &) const;

	using Action = Partition_operations::Action;

	void reset_operation()
	{
		_partition_operations.reset_operation();
	}

	void click(Clicked_at const &at, Storage_device const &device, Storage_target const &used_target, Action &action)
	{
		Id const partition_id = at.matching_id<Vbox, Hbox>();

		Storage_target const selected_target { device.label, device.port, _selected_partition };

		if (partition_id.valid()) {
			_selected_partition = (partition_id.value == _selected_partition)
			                    ? Partition::Number { }
			                    : Partition::Number { partition_id.value };

			_partition_operations.reset_operation();
		}

		_partition_operations.click(at, selected_target, used_target, action);
	}

	void clack(Clacked_at const &at, Storage_device const &device, Action &action)
	{
		Storage_target const selected_target { device.label, device.port, _selected_partition };

		_partition_operations.clack(at, selected_target, action);
	}
};

#endif /* _VIEW__STORAGE_DEVICE_WIDGET_H_ */
