/*
 * \brief  Storage-device management dialog
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
#include "storage_device_dialog.h"

using namespace Sculpt;


void Storage_device_dialog::_gen_partition(Xml_generator        &xml,
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

	if (selected && _partition_dialog.constructed())
		_partition_dialog->gen_operations(xml, device, partition);
}


void Storage_device_dialog::generate(Xml_generator &xml, Storage_device const &dev) const
{
	xml.node("frame", [&] () {
		xml.attribute("name", dev.label);
		xml.attribute("style", "invisible");
		xml.node("vbox", [&] () {
			dev.partitions.for_each([&] (Partition const &partition) {
				_gen_partition(xml, dev, partition); });

			if (!_partition_item.any_selected() && _partition_dialog.constructed())
				_partition_dialog->gen_operations(xml, dev, *dev.whole_device_partition);
		});
	});
}


Dialog::Hover_result Storage_device_dialog::hover(Xml_node hover)
{
	Hover_result result = Hover_result::UNMODIFIED;

	if (_partition_dialog.constructed() &&
		_partition_dialog->match_sub_dialog(hover, "frame", "vbox") == Hover_result::CHANGED)
		result = Hover_result::CHANGED;

	return any_hover_changed(
		result,
		_partition_item.match(hover, "frame", "vbox", "hbox", "name"));
}


Dialog::Click_result Storage_device_dialog::click(Action &action)
{
	Storage_target const orig_selected_target = _selected_storage_target();

	_partition_item.toggle_selection_on_click();

	if (_selected_storage_target() != orig_selected_target) {
		_partition_dialog.construct(_selected_storage_target(),
		                            _storage_devices, _used_target);
		return Click_result::CONSUMED;
	}

	if (_partition_dialog.constructed())
		return _partition_dialog->click(action);

	return Click_result::IGNORED;
}


Dialog::Clack_result Storage_device_dialog::clack(Action &action)
{
	if (_partition_dialog.constructed())
		return _partition_dialog->clack(action);

	return Clack_result::IGNORED;
}

