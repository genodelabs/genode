/*
 * \brief  System dialog
 * \author Norman Feske
 * \date   2023-04-24
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW__SYSTEM_DIALOG_H_
#define _VIEW__SYSTEM_DIALOG_H_

#include <view/layout_helper.h>
#include <view/dialog.h>
#include <view/software_presets_dialog.h>
#include <view/software_update_dialog.h>
#include <view/software_version_dialog.h>
#include <model/settings.h>

namespace Sculpt { struct System_dialog; }


struct Sculpt::System_dialog : Noncopyable, Dialog
{
	using Depot_users = Depot_users_dialog::Depot_users;
	using Image_index = Attached_rom_dataspace;

	Hoverable_item _tab_item { };

	enum Tab { PRESETS, UPDATE } _selected_tab = Tab::PRESETS;

	Software_presets_dialog _presets_dialog;
	Software_update_dialog  _update_dialog;
	Software_version_dialog _version_dialog;

	Hover_result hover(Xml_node hover) override
	{
		Hover_result dialog_hover_result = Hover_result::UNMODIFIED;

		hover.with_optional_sub_node("frame", [&] (Xml_node const &frame) {
			frame.with_optional_sub_node("vbox", [&] (Xml_node const &vbox) {
				switch (_selected_tab) {
				case Tab::PRESETS: dialog_hover_result = _presets_dialog.hover(vbox); break;
				case Tab::UPDATE:  dialog_hover_result = _update_dialog.hover(vbox);  break;
				}
			});
		});

		return any_hover_changed(
			dialog_hover_result,
			_tab_item.match(hover, "frame", "vbox", "hbox", "button", "name"));
	}

	void reset() override { }

	void generate(Xml_generator &xml) const override
	{
		gen_named_node(xml, "frame", "system", [&] {
			xml.node("vbox", [&] {
				gen_named_node(xml, "hbox", "tabs", [&] {
					auto gen_tab = [&] (auto const &id, auto tab, auto const &text)
					{
						gen_named_node(xml, "button", id, [&] {
							_tab_item.gen_hovered_attr(xml, id);
							if (_selected_tab == tab)
								xml.attribute("selected", "yes");
							xml.node("label", [&] {
								xml.attribute("text", text);
							});
						});
					};
					gen_tab("presets", Tab::PRESETS, " Presets ");
					gen_tab("update",  Tab::UPDATE,  " Update ");
				});
				switch (_selected_tab) {
				case Tab::PRESETS:
					_presets_dialog.generate(xml);
					break;
				case Tab::UPDATE: 
					_update_dialog .generate(xml);
					_version_dialog.generate(xml);
					break;
				};
			});
		});
	}

	Click_result click()
	{
		if (_tab_item._hovered.valid()) {
			if (_tab_item._hovered == "presets") _selected_tab = Tab::PRESETS;
			if (_tab_item._hovered == "update")  _selected_tab = Tab::UPDATE;
			return Click_result::CONSUMED;
		};

		switch (_selected_tab) {
		case Tab::PRESETS: _presets_dialog.click(); break;
		case Tab::UPDATE:  _update_dialog .click(); break;
		}
		return Click_result::CONSUMED;
	}

	Click_result clack()
	{
		switch (_selected_tab) {
		case Tab::PRESETS: _presets_dialog.clack(); break;
		case Tab::UPDATE:  _update_dialog .clack(); break;
		};
		return Click_result::CONSUMED;
	}

	System_dialog(Presets                   const &presets,
	              Build_info                const &build_info,
	              Nic_state                 const &nic_state,
	              Download_queue            const &download_queue,
	              Index_update_queue        const &index_update_queue,
	              File_operation_queue      const &file_operation_queue,
	              Depot_users               const &depot_users,
	              Image_index               const &image_index,
	              Software_presets_dialog::Action &presets_action,
	              Depot_users_dialog::Action      &depot_users_action,
	              Software_update_dialog::Action  &update_action)
	:
		_presets_dialog(presets, presets_action),
		_update_dialog(build_info, nic_state, download_queue,
		               index_update_queue, file_operation_queue, depot_users,
		               image_index, depot_users_action, update_action),
		_version_dialog(build_info)
	{ }

	bool update_tab_selected() const { return _selected_tab == Tab::UPDATE; }

	bool keyboard_needed() const { return _update_dialog.keyboard_needed(); }

	void handle_key(Codepoint c) { _update_dialog.handle_key(c); }
};

#endif /* _VIEW__SYSTEM_DIALOG_H_ */
