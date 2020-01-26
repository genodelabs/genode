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
#include <view/selectable_item.h>
#include <view/storage_device_dialog.h>

namespace Sculpt { struct Storage_dialog; }


struct Sculpt::Storage_dialog : Dialog
{
	Storage_devices const &_storage_devices;

	Selectable_item _device_item { };

	Storage_target const &_used_target;

	Constructible<Storage_device_dialog> _storage_device_dialog { };

	void _gen_block_device      (Xml_generator &, Block_device       const &) const;
	void _gen_usb_storage_device(Xml_generator &, Usb_storage_device const &) const;

	void generate(Xml_generator &) const override { };

	void gen_block_devices      (Xml_generator &) const;
	void gen_usb_storage_devices(Xml_generator &) const;

	Hover_result hover(Xml_node hover) override;

	using Action = Storage_device_dialog::Action;

	void reset() override { }

	void reset_operation()
	{
		if (_storage_device_dialog.constructed())
			_storage_device_dialog->reset_operation();
	}

	Click_result click(Action &);
	Clack_result clack(Action &);

	Storage_dialog(Storage_devices const &storage_devices,
	               Storage_target  const &used)
	:
		_storage_devices(storage_devices),
		_used_target(used)
	{ }
};

#endif /* _VIEW__STORAGE_DIALOG_H_ */
