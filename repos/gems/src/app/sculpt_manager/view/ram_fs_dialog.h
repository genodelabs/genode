/*
 * \brief  RAM file-system management dialog
 * \author Norman Feske
 * \date   2020-01-28
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW__RAM_FS_DIALOG_H_
#define _VIEW__RAM_FS_DIALOG_H_

#include <view/fs_dialog.h>
#include <model/ram_fs_state.h>

namespace Sculpt { struct Ram_fs_dialog; }


struct Sculpt::Ram_fs_dialog : Noncopyable, Dialog
{
	Storage_target const &_used_target;

	Storage_target const _target { "ram_fs", Partition::Number() };

	Fs_dialog _fs_dialog;

	Selectable_item  _operation_item { };
	Activatable_item _confirm_item   { };

	Hover_result hover(Xml_node hover) override
	{
		return any_hover_changed(
			_fs_dialog.match_sub_dialog(hover),
			_operation_item.match(hover, "button", "name"),
			_confirm_item  .match(hover, "button", "name"));
	}

	void reset() override
	{
		_operation_item.reset();
		_confirm_item.reset();
	}

	struct Action : Interface, Noncopyable
	{
		virtual void reset_ram_fs() = 0;
	};

	void generate(Xml_generator &) const override { }

	void generate(Xml_generator &xml, Ram_fs_state const &ram_fs_state) const
	{
		_fs_dialog.generate(xml, ram_fs_state);

		if (!_used_target.ram_fs() && !ram_fs_state.inspected) {
			xml.node("button", [&] () {
				_operation_item.gen_button_attr(xml, "reset");

				xml.node("label", [&] () { xml.attribute("text", "Reset ..."); });
			});

			if (_operation_item.selected("reset")) {
				xml.node("button", [&] () {
					_confirm_item.gen_button_attr(xml, "confirm");
					xml.node("label", [&] () { xml.attribute("text", "Confirm"); });
				});
			}
		}
	}

	Click_result click(Fs_dialog::Action &fs_action)
	{
		if (_fs_dialog.click(fs_action) == Click_result::CONSUMED)
			return Click_result::CONSUMED;

		/* toggle confirmation button when clicking on ram_fs reset */
		else if (_operation_item.hovered("reset"))
			_operation_item.toggle_selection_on_click();

		else if (_confirm_item.hovered("confirm"))
			_confirm_item.propose_activation_on_click();

		else return Click_result::IGNORED;

		return Click_result::CONSUMED;
	}

	Clack_result clack(Action &action)
	{
		if (_confirm_item.hovered("confirm")) {

			_confirm_item.confirm_activation_on_clack();

			if (_confirm_item.activated("confirm")) {
				if (_operation_item.selected("reset")) {
					action.reset_ram_fs();
					_operation_item.reset();
					_confirm_item.reset();
					return Clack_result::CONSUMED;
				}
			}
		} else {
			_confirm_item.reset();
		}

		return Clack_result::IGNORED;
	}

	Ram_fs_dialog(Storage_target const &used_target)
	:
		_used_target(used_target), _fs_dialog(_target, used_target)
	{ }
};

#endif /* _VIEW__RAM_FS_DIALOG_H_ */
