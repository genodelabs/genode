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

/* Genode includes */
#include <base/log.h>

/* local includes */
#include "partition_dialog.h"

using namespace Sculpt;


void Partition_dialog::gen_operations(Xml_generator        &xml,
                                      Storage_device const &device,
                                      Partition      const &partition) const
{
	String<16> const version(device.label, ".", partition.number);

	bool const whole_device = !partition.number.valid();

	bool const device_in_use = (_used_target.device == device.label);

	bool const target_in_use = (_used_target == _partition)
	                        || (whole_device && device_in_use)
	                        || partition.file_system.inspected;

	bool const relabel_in_progress = device.relabel_in_progress();

	bool const expand_in_progress = device.expand_in_progress();

	bool const format_selected = _operation_item.selected("format");

	bool const expand_selected = _operation_item.selected("expand");

	if (partition.file_system.accessible()
	 && !format_selected && !expand_selected && !expand_in_progress) {

		if (!partition.check_in_progress
		 && !partition.format_in_progress
		 && !relabel_in_progress) {

			_fs_dialog.generate(xml, partition.file_system);
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
				xml.node("label", [&] () { xml.attribute("text", "Format device ..."); });
			} else {
				xml.node("label", [&] () { xml.attribute("text", "Format partition ..."); });
			}
		});
	}

	if (expand_button_visible) {
		xml.node("button", [&] () {
			_operation_item.gen_button_attr(xml, "expand");
			xml.attribute("version", version);

			if (partition.expand_in_progress())
				xml.attribute("selected", "yes");

			xml.node("label", [&] () { xml.attribute("text", "Expand ..."); });
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


Dialog::Hover_result Partition_dialog::hover(Xml_node hover)
{
	Hover_result const hover_result = any_hover_changed(
		_fs_dialog.hover(hover),
		_relabel_item  .match(hover, "button", "name"),
		_operation_item.match(hover, "button", "name"),
		_confirm_item  .match(hover, "button", "name"));

	return hover_result;
}


Dialog::Click_result Partition_dialog::click(Action &action)
{
	if (_fs_dialog.click(action) == Click_result::CONSUMED)
		return Click_result::CONSUMED;

	if (_operation_item.hovered("format")) {
		if (_operation_item.selected("format"))
			action.cancel_format(_partition);
		else
			_operation_item.toggle_selection_on_click();
		return Click_result::CONSUMED;
	}

	if (_operation_item.hovered("expand")) {
		if (_operation_item.selected("expand"))
			action.cancel_expand(_partition);
		else
			_operation_item.toggle_selection_on_click();
		return Click_result::CONSUMED;
	}

	if (_operation_item.hovered("check")) {
		action.check(_partition);
		return Click_result::CONSUMED;
	}

	if (_relabel_item.hovered("relabel")) {
		action.toggle_default_storage_target(_partition);
		return Click_result::CONSUMED;
	}

	if (_confirm_item.hovered("confirm")) {
		_confirm_item.propose_activation_on_click();
		return Click_result::CONSUMED;
	}

	return Click_result::IGNORED;
}


Dialog::Clack_result Partition_dialog::clack(Action &action)
{
	if (_confirm_item.hovered("confirm")) {

		_confirm_item.confirm_activation_on_clack();

		if (_confirm_item.activated("confirm")) {

			if (_operation_item.selected("format")) {
				action.format(_partition);
				return Clack_result::CONSUMED;
			}

			if (_operation_item.selected("expand")) {
				action.expand(_partition);
				return Clack_result::CONSUMED;
			}
		}
	} else {
		_confirm_item.reset();
	}

	return Clack_result::IGNORED;
}

