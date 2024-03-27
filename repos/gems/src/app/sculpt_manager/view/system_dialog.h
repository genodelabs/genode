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

#include <view/dialog.h>
#include <view/software_presets_widget.h>
#include <view/software_update_widget.h>
#include <view/software_version_widget.h>
#include <model/settings.h>

namespace Sculpt { struct System_dialog; }


struct Sculpt::System_dialog : Top_level_dialog
{
	using Depot_users = Depot_users_widget::Depot_users;
	using Image_index = Rom_data;

	Presets     const &_presets;
	Image_index const &_image_index;
	Build_info  const &_build_info;

	Software_presets_widget::Action &_presets_action;
	Software_update_widget::Action  &_update_action;

	enum Tab { PRESET, UPDATE } _selected_tab = Tab::PRESET;

	Hosted<Frame, Vbox, Software_presets_widget> _presets_widget { Id { "presets" } };
	Hosted<Frame, Vbox, Software_update_widget>  _update_widget;
	Hosted<Frame, Vbox, Software_version_widget> _version_widget { Id { "version" } };

	Hosted<Frame, Vbox, Hbox, Select_button<Tab>>
		_preset_tab { Id { " Presets " }, Tab::PRESET },
		_update_tab { Id { " Update "  }, Tab::UPDATE };

	void view(Scope<> &s) const override
	{
		s.sub_scope<Frame>([&] (Scope<Frame> &s) {
			s.sub_scope<Vbox>([&] (Scope<Frame, Vbox> &s) {

				/* tabs */
				s.sub_scope<Hbox>([&] (Scope<Frame, Vbox, Hbox> &s) {
					s.widget(_preset_tab, _selected_tab, [&] (auto &s) {
						if (!_presets.available())
							s.attribute("style", "unimportant");
						s.template sub_scope<Label>(_preset_tab.id.value);
					});
					s.widget(_update_tab, _selected_tab);
				});

				switch (_selected_tab) {
				case Tab::PRESET:
					s.widget(_presets_widget, _presets);
					break;
				case Tab::UPDATE:
					_image_index.with_xml([&] (Xml_node const &index) {
						s.widget(_update_widget, index); });
					s.widget(_version_widget, _build_info);
					break;
				};
			});
		});
	}

	void click(Clicked_at const &at) override
	{
		_preset_tab.propagate(at, [&] (Tab t) { _selected_tab = t; });
		_update_tab.propagate(at, [&] (Tab t) { _selected_tab = t; });

		_presets_widget.propagate(at, _presets);

		if (_selected_tab == Tab::UPDATE)
			_update_widget.propagate(at, _update_action);
	}

	void clack(Clacked_at const &at) override
	{
		_presets_widget.propagate(at, _presets, _presets_action);
	}

	void drag(Dragged_at const &) override { }

	System_dialog(Presets                   const &presets,
	              Build_info                const &build_info,
	              Nic_state                 const &nic_state,
	              Download_queue            const &download_queue,
	              Index_update_queue        const &index_update_queue,
	              File_operation_queue      const &file_operation_queue,
	              Depot_users               const &depot_users,
	              Image_index               const &image_index,
	              Software_presets_widget::Action &presets_action,
	              Software_update_widget::Action  &update_action)
	:
		Top_level_dialog("system"),
		_presets(presets), _image_index(image_index), _build_info(build_info),
		_presets_action(presets_action), _update_action(update_action),
		_update_widget(Id { "update" },
		               build_info, nic_state, download_queue,
		               index_update_queue, file_operation_queue, depot_users)
	{ }

	bool update_tab_selected() const { return _selected_tab == Tab::UPDATE; }

	bool keyboard_needed() const { return _update_widget.keyboard_needed(); }

	void handle_key(Codepoint c, Software_update_widget::Action &action)
	{
		_update_widget.handle_key(c, action);
	}

	void sanitize_user_selection() { _update_widget.sanitize_user_selection(); }

	void reset_update_widget() { _update_widget.reset(); }
};

#endif /* _VIEW__SYSTEM_DIALOG_H_ */
