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

using namespace Sculpt;


void Storage_dialog::_gen_block_device(Xml_generator      &xml,
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

	if (_storage_device_dialog.constructed() && selected)
		_storage_device_dialog->generate(xml, dev);
}


void Storage_dialog::_gen_usb_storage_device(Xml_generator            &xml,
                                             Usb_storage_device const &dev) const
{
	bool const discarded = dev.discarded();
	bool const selected  = !discarded && _device_item.selected(dev.label);

	xml.node("button", [&] () {
		xml.attribute("name", dev.label);

		if (_device_item.hovered(dev.label) && !discarded)
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

			typedef String<64> Info;
			Info const info = dev.discarded() ? Info(" unsupported")
			                                  : Info(" ", dev.capacity);

			gen_named_node(xml, "float", "capacity", [&] () {
				xml.attribute("east", "yes");
				xml.node("label", [&] () {
					xml.attribute("text", info); }); });
		});
	});

	if (_storage_device_dialog.constructed() && selected)
		_storage_device_dialog->generate(xml, dev);
}


void Storage_dialog::gen_block_devices(Xml_generator &xml) const
{
	_storage_devices.block_devices.for_each([&] (Block_device const &dev) {
		_gen_block_device(xml, dev); });
}


void Storage_dialog::gen_usb_storage_devices(Xml_generator &xml) const
{
	_storage_devices.usb_storage_devices.for_each([&] (Usb_storage_device const &dev) {
		_gen_usb_storage_device(xml, dev); });
}


Dialog::Hover_result Storage_dialog::hover(Xml_node hover)
{
	Hover_result result = Hover_result::UNMODIFIED;

	if (_storage_device_dialog.constructed() &&
	    _storage_device_dialog->hover(hover) == Hover_result::CHANGED)
		result = Hover_result::CHANGED;

	return any_hover_changed(result, _device_item.match(hover, "button", "name"));
}


Dialog::Click_result Storage_dialog::click(Action &action)
{
	Selectable_item::Id const old_selected_device = _device_item._selected;

	_device_item.toggle_selection_on_click();

	if (old_selected_device != _device_item._selected) {

		_storage_device_dialog.destruct();
		_storage_device_dialog.conditional(_device_item.any_selected(),
		                                   _device_item._selected,
		                                   _storage_devices, _used_target);
		return Click_result::CONSUMED;
	}

	if (_storage_device_dialog.constructed()
	 && _storage_device_dialog->click(action) == Click_result::CONSUMED) {

		return Click_result::CONSUMED;
	}

	return Click_result::IGNORED;
}


Dialog::Clack_result Storage_dialog::clack(Action &action)
{
	if (_storage_device_dialog.constructed()
	 && _storage_device_dialog->clack(action) == Clack_result::CONSUMED)
		return Clack_result::CONSUMED;

	return Clack_result::IGNORED;
}

