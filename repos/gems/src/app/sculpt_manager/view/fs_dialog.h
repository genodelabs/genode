/*
 * \brief  Common part of file-system management dialogs
 * \author Norman Feske
 * \date   2020-01-28
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW__FS_DIALOG_H_
#define _VIEW__FS_DIALOG_H_

#include <view/dialog.h>
#include <model/storage_target.h>

namespace Sculpt { struct Fs_dialog; }


struct Sculpt::Fs_dialog : Noncopyable, Dialog
{
	Storage_target const  _target;
	Storage_target const &_used_target;

	Hoverable_item _inspect_item   { };
	Hoverable_item _use_item       { };

	Hover_result hover(Xml_node hover) override
	{
		return any_hover_changed(
			_inspect_item.match(hover, "button", "name"),
			_use_item    .match(hover, "button", "name"));
	}

	void reset() override { }

	struct Action : Interface
	{
		virtual void toggle_inspect_view(Storage_target const &) = 0;

		virtual void use(Storage_target const &) = 0;
	};

	void generate(Xml_generator &) const override { }

	void generate(Xml_generator &xml, File_system const &file_system) const
	{
		xml.node("button", [&] () {
			_inspect_item.gen_button_attr(xml, "browse");

			if (file_system.inspected)
				xml.attribute("selected", "yes");

			xml.node("label", [&] () { xml.attribute("text", "Inspect"); });
		});

		if (!_used_target.valid() || _used_target == _target) {
			xml.node("button", [&] () {
				_use_item.gen_button_attr(xml, "use");
				if (_used_target == _target)
					xml.attribute("selected", "yes");
				xml.node("label", [&] () { xml.attribute("text", "Use"); });
			});
		}
	}

	Click_result click(Action &action)
	{
		if (_inspect_item.hovered("browse"))
			action.toggle_inspect_view(_target);

		else if (_use_item.hovered("use"))
			action.use((_used_target == _target) ? Storage_target{ } : _target);

		else return Click_result::IGNORED;

		return Click_result::CONSUMED;
	}

	Fs_dialog(Storage_target target, Storage_target const &used_target)
	:
		_target(target), _used_target(used_target)
	{ }
};

#endif /* _VIEW__FS_DIALOG_H_ */
