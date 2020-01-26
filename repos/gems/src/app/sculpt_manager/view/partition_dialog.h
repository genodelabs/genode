/*
 * \brief  Partition management dialog
 * \author Norman Feske
 * \date   2020-01-29
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW__PARTITION_DIALOG_H_
#define _VIEW__PARTITION_DIALOG_H_

#include <types.h>
#include <model/storage_devices.h>
#include <model/storage_target.h>
#include <view/selectable_item.h>
#include <view/activatable_item.h>
#include <view/fs_dialog.h>

namespace Sculpt { struct Partition_dialog; }


struct Sculpt::Partition_dialog : Dialog
{
	Storage_target  const  _partition;
	Storage_devices const &_storage_devices;
	Storage_target  const &_used_target;

	Hoverable_item   _relabel_item   { };
	Selectable_item  _operation_item { };
	Activatable_item _confirm_item   { };

	Fs_dialog _fs_dialog { _partition, _used_target };

	void generate(Xml_generator &) const override { }

	void gen_operations(Xml_generator &, Storage_device const &, Partition const &) const;

	Hover_result hover(Xml_node hover) override;

	struct Action : Fs_dialog::Action
	{
		virtual void format(Storage_target const &) = 0;

		virtual void cancel_format(Storage_target const &) = 0;

		virtual void expand(Storage_target const &) = 0;

		virtual void cancel_expand(Storage_target const &) = 0;

		virtual void check(Storage_target const &) = 0;

		virtual void toggle_default_storage_target(Storage_target const &) = 0;
	};

	void reset() override { }

	void reset_operation() { _operation_item.reset(); }

	Click_result click(Action &action);
	Clack_result clack(Action &action);

	Partition_dialog(Storage_target  const  partition,
	                 Storage_devices const &storage_devices,
	                 Storage_target  const &used)
	:
		_partition(partition),
		_storage_devices(storage_devices),
		_used_target(used)
	{ }
};

#endif /* _VIEW__PARTITION_DIALOG_H_ */
