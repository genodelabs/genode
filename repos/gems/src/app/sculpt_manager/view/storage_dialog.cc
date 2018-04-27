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

/* Genode includes */
#include <base/log.h>

/* local includes */
#include "storage_dialog.h"


void Sculpt::Storage_dialog::_gen_partition_operations(Xml_generator        &xml,
                                                       Storage_device const &device,
                                                       Partition      const &partition) const
{
	Storage_target const target { device.label, partition.number };

	String<16> const version(device.label, ".", partition.number);

	bool const whole_device = !partition.number.valid();

	bool const device_in_use = (_used_target.device == device.label);

	bool const target_in_use = (_used_target == target)
	                        || (whole_device && device_in_use)
	                        || partition.file_system_inspected;

	bool const relabel_in_progress = device.relabel_in_progress();

	bool const expand_in_progress = device.expand_in_progress();

	bool const format_selected = _operation_item.selected("format");

	bool const expand_selected = _operation_item.selected("expand");

	if (partition.file_system.accessible() && !format_selected
	 && !expand_selected && !expand_in_progress) {

		if (!partition.check_in_progress && !partition.format_in_progress
		 && partition.file_system.accessible() && !relabel_in_progress) {

			xml.node("button", [&] () {
				_inspect_item.gen_button_attr(xml, "browse");
				xml.attribute("version", version);

				if (partition.file_system_inspected)
					xml.attribute("selected", "yes");

				xml.node("label", [&] () { xml.attribute("text", "Inspect"); });
			});
		}

		if ((!_used_target.valid() || _used_target == target)
		 && !partition.check_in_progress && !partition.format_in_progress
		 && !relabel_in_progress) {

			xml.node("button", [&] () {
				xml.attribute("version", version);
				_use_item.gen_button_attr(xml, "use");
				if (_used_target == target)
					xml.attribute("selected", "yes");
				xml.node("label", [&] () { xml.attribute("text", "Use"); });
			});
		}

		if ((device.all_partitions_idle() || partition.relabel_in_progress())
		  && partition.genode() && !device_in_use) {

			xml.node("button", [&] () {

				/* support hovering only if no relabeling is in progress */
				if (partition.relabel_in_progress())
					xml.attribute("name", "relabel");
				else
					_relabel_item.gen_button_attr(xml, "relabel");

				xml.attribute("version", version);

				if (partition.genode_default() || partition.relabel_in_progress())
					xml.attribute("selected", "yes");

				xml.node("label", [&] () {
					xml.attribute("text", "Default"); });
			});
			if (partition.relabel_in_progress())
				xml.node("label", [&] () { xml.attribute("text", "In progress..."); });
		}

		if (!target_in_use && !partition.format_in_progress && partition.checkable()
		  && !relabel_in_progress) {

			xml.node("button", [&] () {
				_operation_item.gen_button_attr(xml, "check");
				xml.attribute("version", version);

				if (partition.check_in_progress)
					xml.attribute("selected", "yes");

				xml.node("label", [&] () { xml.attribute("text", "Check"); });
			});
			if (partition.check_in_progress)
				xml.node("label", [&] () { xml.attribute("text", "In progress..."); });
		}
	}

	bool const whole_device_with_partition_in_use =
		whole_device && !device.all_partitions_idle();

	bool const format_button_visible = !target_in_use
	                                && !whole_device_with_partition_in_use
	                                && !partition.check_in_progress
	                                && !expand_in_progress
	                                && !relabel_in_progress
	                                && !_operation_item.selected("expand");

	bool const expand_button_visible = !target_in_use
	                                && !whole_device
	                                && !partition.check_in_progress
	                                && !partition.format_in_progress
	                                && !relabel_in_progress
	                                && partition.expandable()
	                                && !_operation_item.selected("format");

	bool const progress_msg_visible =
		   (_operation_item.selected("format") && partition.format_in_progress)
		|| (_operation_item.selected("expand") && partition.expand_in_progress());

	bool const confirm_visible =
		   (_operation_item.selected("format") && !partition.format_in_progress)
		|| (_operation_item.selected("expand") && !partition.expand_in_progress());

	if (format_button_visible) {
		xml.node("button", [&] () {
			_operation_item.gen_button_attr(xml, "format");
			xml.attribute("version", version);

			if (partition.format_in_progress)
				xml.attribute("selected", "yes");

			if (whole_device) {
				xml.node("label", [&] () { xml.attribute("text", "Format device"); });
			} else {
				xml.node("label", [&] () { xml.attribute("text", "Format partition"); });
			}
		});
	}

	if (expand_button_visible) {
		xml.node("button", [&] () {
			_operation_item.gen_button_attr(xml, "expand");
			xml.attribute("version", version);

			if (partition.expand_in_progress())
				xml.attribute("selected", "yes");

			xml.node("label", [&] () { xml.attribute("text", "Expand"); });
		});
	}

	if (progress_msg_visible)
		xml.node("label", [&] () { xml.attribute("text", "In progress..."); });

	if (confirm_visible) {
		xml.node("button", [&] () {
			_confirm_item.gen_button_attr(xml, "confirm");
			xml.attribute("version", version);
			xml.node("label", [&] () { xml.attribute("text", "Confirm"); });
		});
	}
}


void Sculpt::Storage_dialog::_gen_partition(Xml_generator        &xml,
                                            Storage_device const &device,
                                            Partition      const &partition) const
{
	bool const selected = _partition_item.selected(partition.number);

	gen_named_node(xml, "hbox", partition.number, [&] () {
		gen_named_node(xml, "float", "left", [&] () {
			xml.attribute("west", "yes");
			xml.node("hbox", [&] () {
				gen_named_node(xml, "button", "button", [&] () {

					if (_partition_item.hovered(partition.number))
						xml.attribute("hovered", "yes");

					if (selected)
						xml.attribute("selected", "yes");

					xml.node("label", [&] () { xml.attribute("text", partition.number); });
				});

				if (partition.label.length() > 1)
					gen_named_node(xml, "label", "label", [&] () {
						xml.attribute("text", String<80>(" (", partition.label, ") ")); });

				Storage_target const target { device.label, partition.number };
				if (_used_target == target)
					gen_named_node(xml, "label", "used", [&] () { xml.attribute("text", "* "); });
			});
		});

		gen_named_node(xml, "float", "right", [&] () {
			xml.attribute("east", "yes");
			xml.node("label", [&] () {
				xml.attribute("text", String<64>(partition.capacity, " ")); });
		});
	});

	if (selected)
		_gen_partition_operations(xml, device, partition);
}


void Sculpt::Storage_dialog::_gen_block_device(Xml_generator      &xml,
                                               Block_device const &dev) const
{
	bool const selected = _device_item.selected(dev.label);

	xml.node("button", [&] () {
		xml.attribute("name", dev.label);

		if (_device_item.hovered(dev.label))
			xml.attribute("hovered", "yes");

		if (selected)
			xml.attribute("selected", "yes");

		xml.node("hbox", [&] () {
			gen_named_node(xml, "float", "info", [&] () {
				xml.attribute("west", "yes");
				xml.node("hbox", [&] () {
					gen_named_node(xml, "label", "device", [&] () {
						xml.attribute("text", dev.label); });
					gen_named_node(xml, "label", "model", [&] () {
						xml.attribute("text", String<80>(" (", dev.model, ") ")); });
					if (_used_target.device == dev.label)
						gen_named_node(xml, "label", "used", [&] () {
							xml.attribute("text", "* "); });
				});
			});

			gen_named_node(xml, "float", "capacity", [&] () {
				xml.attribute("east", "yes");
				xml.node("label", [&] () {
					xml.attribute("text", String<64>(dev.capacity)); }); });
		});
	});

	if (selected) {
		xml.node("frame", [&] () {
			xml.attribute("name", dev.label);
			xml.node("vbox", [&] () {
				dev.partitions.for_each([&] (Partition const &partition) {
					_gen_partition(xml, dev, partition); });

				if (!_partition_item.any_selected())
					_gen_partition_operations(xml, dev, *dev.whole_device_partition);
			});
		});
	}
}


void Sculpt::Storage_dialog::_gen_usb_storage_device(Xml_generator            &xml,
                                                     Usb_storage_device const &dev) const
{
	bool const selected = _device_item.selected(dev.label);

	xml.node("button", [&] () {
		xml.attribute("name", dev.label);

		if (_device_item.hovered(dev.label))
			xml.attribute("hovered", "yes");

		if (selected)
			xml.attribute("selected", "yes");

		xml.node("hbox", [&] () {
			gen_named_node(xml, "float", "info", [&] () {
				xml.attribute("west", "yes");

				xml.node("hbox", [&] () {

					gen_named_node(xml, "label", "device", [&] () {
						xml.attribute("text", dev.label); });

					if (dev.driver_info.constructed()) {
						gen_named_node(xml, "label", "vendor", [&] () {
							String<16> const vendor  { dev.driver_info->vendor };
							xml.attribute("text", String<64>(" (", vendor, ") ")); });
					}

					if (_used_target.device == dev.label)
						gen_named_node(xml, "label", "used", [&] () {
							xml.attribute("text", " *"); });
				});
			});

			gen_named_node(xml, "float", "capacity", [&] () {
				xml.attribute("east", "yes");
				xml.node("label", [&] () {
					xml.attribute("text", String<64>(dev.capacity)); }); });
		});
	});

	if (selected)
		xml.node("frame", [&] () {
			xml.attribute("name", dev.label);
			xml.node("vbox", [&] () {
				dev.partitions.for_each([&] (Partition const &partition) {
					_gen_partition(xml, dev, partition); });

				if (!_partition_item.any_selected())
					_gen_partition_operations(xml, dev, *dev.whole_device_partition);
			});
		});
}


void Sculpt::Storage_dialog::_gen_ram_fs(Xml_generator &xml) const
{
	bool const selected = _device_item.selected("ram_fs");

	gen_named_node(xml, "button", "ram_fs", [&] () {

		if (_device_item.hovered("ram_fs"))
			xml.attribute("hovered", "yes");

		if (selected)
			xml.attribute("selected", "yes");

		xml.node("hbox", [&] () {
			gen_named_node(xml, "float", "info", [&] () {
				xml.attribute("west", "yes");

				xml.node("hbox", [&] () {

					gen_named_node(xml, "label", "device", [&] () {
						xml.attribute("text", "ram (in-memory file system) "); });

					if (_used_target.device == "ram_fs")
						gen_named_node(xml, "label", "used", [&] () {
							xml.attribute("text", "* "); });
				});
			});

			gen_named_node(xml, "float", "capacity", [&] () {
				xml.attribute("east", "yes");
				xml.node("label", [&] () {
					Capacity const capacity { _ram_fs_state.ram_quota.value };
					xml.attribute("text", String<64>(capacity)); }); });
		});
	});

	if (selected) {
		xml.node("frame", [&] () {
			xml.attribute("name", "ram_fs_operations");
			xml.node("vbox", [&] () {

				xml.node("button", [&] () {
					_inspect_item.gen_button_attr(xml, "browse");

					if (_ram_fs_state.inspected)
						xml.attribute("selected", "yes");

					xml.node("label", [&] () { xml.attribute("text", "Inspect"); });
				});

				if (!_used_target.valid() || _used_target.ram_fs()) {
					xml.node("button", [&] () {
						_use_item.gen_button_attr(xml, "use");
						if (_used_target.ram_fs())
							xml.attribute("selected", "yes");
						xml.node("label", [&] () { xml.attribute("text", "Use"); });
					});
				}

				if (!_used_target.ram_fs() && !_ram_fs_state.inspected) {
					xml.node("button", [&] () {
						_operation_item.gen_button_attr(xml, "reset");

						xml.node("label", [&] () { xml.attribute("text", "Reset"); });
					});

					if (_operation_item.selected("reset")) {
						xml.node("button", [&] () {
							_confirm_item.gen_button_attr(xml, "confirm");
							xml.node("label", [&] () { xml.attribute("text", "Confirm"); });
						});
					}
				}
			});
		});
	}
}


void Sculpt::Storage_dialog::generate(Xml_generator &xml) const
{
	gen_named_node(xml, "frame", "storage", [&] () {
		xml.node("vbox", [&] () {
			gen_named_node(xml, "label", "title", [&] () {
				xml.attribute("text", "Storage");
				xml.attribute("font", "title/regular");
			});
			_storage_devices.block_devices.for_each([&] (Block_device const &dev) {
				_gen_block_device(xml, dev); });
			_storage_devices.usb_storage_devices.for_each([&] (Usb_storage_device const &dev) {
				_gen_usb_storage_device(xml, dev); });

			_gen_ram_fs(xml);
		});
	});
}


void Sculpt::Storage_dialog::hover(Xml_node hover)
{
	bool const changed =
		_device_item   .match(hover, "vbox", "button", "name") |
		_partition_item.match(hover, "vbox", "frame", "vbox", "hbox", "name") |
		_use_item      .match(hover, "vbox", "frame", "vbox", "button", "name") |
		_relabel_item  .match(hover, "vbox", "frame", "vbox", "button", "name") |
		_inspect_item  .match(hover, "vbox", "frame", "vbox", "button", "name") |
		_operation_item.match(hover, "vbox", "frame", "vbox", "button", "name") |
		_confirm_item  .match(hover, "vbox", "frame", "vbox", "button", "name");

	if (changed)
		_dialog_generator.generate_dialog();
}


void Sculpt::Storage_dialog::click(Action &action)
{
	Selectable_item::Id const old_selected_device    = _device_item._selected;
	Selectable_item::Id const old_selected_partition = _partition_item._selected;

	_device_item.toggle_selection_on_click();
	_partition_item.toggle_selection_on_click();

	if (!_device_item.selected(old_selected_device))
		_partition_item.reset();

	if (!_device_item.selected(old_selected_device)
	 || !_partition_item.selected(old_selected_partition))
		reset_operation();

	Storage_target const target = _selected_storage_target();

	if (_operation_item.hovered("format")) {
		if (_operation_item.selected("format"))
			action.cancel_format(_selected_storage_target());
		else
			_operation_item.toggle_selection_on_click();
	}

	if (_operation_item.hovered("expand")) {
		if (_operation_item.selected("expand"))
			action.cancel_expand(_selected_storage_target());
		else
			_operation_item.toggle_selection_on_click();
	}

	/* toggle confirmation button when clicking on ram_fs reset */
	if (_operation_item.hovered("reset"))
		_operation_item.toggle_selection_on_click();

	if (_operation_item.hovered("check"))
		action.check(target);

	/* toggle file browser */
	if (_inspect_item.hovered("browse"))
		action.toggle_file_browser(target);

	if (_use_item.hovered("use")) {

		/* release currently used device */
		if (target.valid() && target == _used_target)
			action.use(Storage_target{});

		/* select new used device if none is defined */
		else if (!_used_target.valid())
			action.use(target);
	}

	if (_relabel_item.hovered("relabel"))
		action.toggle_default_storage_target(target);

	if (_confirm_item.hovered("confirm")) {
		_confirm_item.propose_activation_on_click();
	}

	_dialog_generator.generate_dialog();
}


void Sculpt::Storage_dialog::clack(Action &action)
{
	if (_confirm_item.hovered("confirm")) {

		_confirm_item.confirm_activation_on_clack();

		if (_confirm_item.activated("confirm")) {

			Storage_target const target = _selected_storage_target();

			if (_operation_item.selected("format"))
				action.format(target);

			if (_operation_item.selected("expand"))
				action.expand(target);

			if (target.ram_fs() && _operation_item.selected("reset"))
				action.reset_ram_fs();
		}
	} else {
		_confirm_item.reset();
	}

	_dialog_generator.generate_dialog();
}

