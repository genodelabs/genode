/*
 * \brief  Graph view of runtime state
 * \author Norman Feske
 * \date   2018-07-05
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <graph.h>
#include <feature.h>
#include <view/dialog.h>

using namespace Sculpt;


namespace Dialog { struct Parent_node; }

struct Dialog::Parent_node : Sub_scope
{
	static void view_sub_scope(auto &s, auto const &text)
	{
		s.node("frame", [&] {
			s.sub_node("label", [&] {
				s.attribute("text", Sculpt::Start_name(" ", text, " ")); }); });
	}

	static void with_narrowed_at(auto const &, auto const &) { }
};


namespace Dialog { struct Selectable_node; }

struct Dialog::Selectable_node
{
	struct Attr
	{
		bool selected;
		bool important;
		Dialog::Id primary_dep;
		Start_name pretty_name;
	};

	static void view(Scope<Depgraph> &s, Id const &id,
	                 Attr const &attr, auto const &selected_fn)
	{
		s.sub_scope<Frame>(id, [&] (Scope<Depgraph, Frame> &s) {

			if (!attr.important)
				s.attribute("style", "unimportant");

			if (attr.primary_dep.valid()) {
				s.attribute("dep", attr.primary_dep.value);
				if (!attr.important)
					s.attribute("dep_visible", false);
			}

			s.sub_scope<Vbox>([&] (Scope<Depgraph, Frame, Vbox> &s) {

				s.sub_scope<Button>(id, [&] (Scope<Depgraph, Frame, Vbox, Button> &s) {

					if (!attr.important) s.attribute("style",    "unimportant");
					if (s.hovered())     s.attribute("hovered",  "yes");
					if (attr.selected)   s.attribute("selected", "yes");

					s.sub_scope<Label>(attr.pretty_name);
				});

				if (attr.selected)
					selected_fn(s);
			});
		});
	}
};


void Graph::_view_selected_node_content(Scope<Depgraph, Frame, Vbox> &s,
                                        Start_name const &name,
                                        Runtime_state::Info const &info) const
{
	if (_deploy_children.exists(name)) {

		s.sub_scope<Frame>([&] (Scope<Depgraph, Frame, Vbox, Frame> &s) {
			s.sub_scope<Hbox>([&] (Scope<Depgraph, Frame, Vbox, Frame, Hbox> &s) {
				s.widget(_remove);
				s.widget(_restart); }); });

	} else if (name == "nic" ||
	           name == "wifi") {

		s.sub_scope<Frame>([&] (Scope<Depgraph, Frame, Vbox, Frame> &s) {
			s.sub_scope<Hbox>([&] (Scope<Depgraph, Frame, Vbox, Frame, Hbox> &s) {
				s.widget(_restart); }); });
	}

	if (name == "ram_fs")
		s.widget(_ram_fs_widget, _selected_target, _ram_fs_state);

	String<100> const
		ram (Capacity{info.assigned_ram - info.avail_ram}, " / ",
		     Capacity{info.assigned_ram}),
		caps(info.assigned_caps - info.avail_caps, " / ",
		     info.assigned_caps, " caps");

	s.sub_scope<Min_ex>(25);
	s.sub_scope<Label>(ram);
	s.sub_scope<Label>(caps);

	if ((name == "usb") && _storage_devices.num_usb_devices)
		s.sub_scope<Frame>([&] (Scope<Depgraph, Frame, Vbox, Frame> &s) {
			s.widget(_usb_devices_widget); });

	if (name == "ahci")
		s.sub_scope<Frame>([&] (Scope<Depgraph, Frame, Vbox, Frame> &s) {
			s.widget(_ahci_devices_widget); });

	if (name == "nvme")
		s.sub_scope<Frame>([&] (Scope<Depgraph, Frame, Vbox, Frame> &s) {
			s.widget(_nvme_devices_widget); });

	if (name == "mmc")
		s.sub_scope<Frame>([&] (Scope<Depgraph, Frame, Vbox, Frame> &s) {
			s.widget(_mmc_devices_widget); });
}


void Graph::view(Scope<Depgraph> &s) const
{
	if (Feature::PRESENT_PLUS_MENU && _selected_target.valid())
		s.widget(_plus, _popup_state == Popup::VISIBLE);

	/* parent roles */
	s.sub_scope<Parent_node>(Id { "hardware" }, "Hardware");
	s.sub_scope<Parent_node>(Id { "config" },   "Config");
	s.sub_scope<Parent_node>(Id { "info" },     "Info");
	s.sub_scope<Parent_node>(Id { "GUI" },      "GUI");

	using Component = Runtime_config::Component;

	bool const any_selected = _runtime_state.selected().valid();

	_runtime_config.for_each_component([&] (Component const &component) {

		Start_name const name = component.name;
		Start_name pretty_name { Pretty(name) };

		if (name == "mmcblk0.part_block")
			pretty_name = "mmcblk0";

		if (name == "mmcblk0.1.fs")
			pretty_name = "1.fs";

		/* omit sculpt's helpers from the graph */
		bool const hidden = (name == "runtime_view"
		                  || name == "popup_view"
		                  || name == "main_view"
		                  || name == "panel_view"
		                  || name == "settings_view"
		                  || name == "network_view"
		                  || name == "file_browser_view"
		                  || name == "editor"
		                  || name == "launcher_query"
		                  || name == "update"
		                  || name == "fs_tool"
		                  || name == "depot_rw"
		                  || name == "public_rw"
		                  || name == "depot_rom"
		                  || name == "dynamic_depot_rom"
		                  || name == "depot_query"
		                  || name == "system_view"
		                  || name == "diag_view"
		                  || name == "manager_keyboard");
		if (hidden)
			return;

		Runtime_state::Info const info = _runtime_state.info(name);

		bool const unimportant = any_selected && !info.tcb;

		/* basic categories, like GUI */
		Dialog::Id primary_dep = Id { component.primary_dependency };

		if (primary_dep.value == "default_fs_rw")
			primary_dep = Dialog::Id { _selected_target.fs() };

		/* primary dependency is another component */
		_runtime_config.with_graph_id(primary_dep,
			[&] (Dialog::Id const &id) { primary_dep = id; });

		Selectable_node::view(s, component.graph_id,
			{
				.selected    = info.selected,
				.important   = !unimportant,
				.primary_dep = primary_dep,
				.pretty_name = pretty_name
			},
			[&] (Scope<Depgraph, Frame, Vbox> &s) {
				_view_selected_node_content(s, name, info);
			}
		);
	});

	_runtime_config.for_each_component([&] (Component const &component) {

		Start_name const name = component.name;

		if (name == "ram_fs")
			return;

		Runtime_state::Info const info = _runtime_state.info(name);

		bool const show_details = info.tcb;

		if (show_details) {
			component.for_each_secondary_dep([&] (Start_name dep_name) {

				if (Runtime_state::blacklisted_from_graph(dep_name))
					return;

				if (dep_name == "default_fs_rw")
					dep_name = _selected_target.fs();

				Dialog::Id dep_id { dep_name };

				_runtime_config.with_graph_id(dep_name, [&] (Dialog::Id const &id) {
					dep_id = id; });

				s.node("dep", [&] {
					s.attribute("node", component.graph_id.value);
					s.attribute("on",   dep_id.value);
				});
			});
		}
	});
}


void Graph::click(Clicked_at const &at, Action &action)
{
	/* select node */
	Id const id = at.matching_id<Depgraph, Frame, Vbox, Button>();
	if (id.valid())
		_runtime_config.with_start_name(id, [&] (Start_name const &name) {
			_runtime_state.toggle_selection(name, _runtime_config); });

	_plus.propagate(at, [&] {

		auto popup_anchor = [] (Xml_node const dialog)
		{
			Rect result { };
			dialog.with_optional_sub_node("depgraph", [&] (Xml_node const &depgraph) {
				depgraph.with_optional_sub_node("button", [&] (Xml_node const &button) {
					result = Rect(Point::from_xml(dialog) + Point::from_xml(depgraph) +
					              Point::from_xml(button),
					              Area::from_xml(button)); });
			});
			return result;
		};

		action.open_popup_dialog(popup_anchor(at._location));
	});

	_ram_fs_widget      .propagate(at, _selected_target, action);
	_ahci_devices_widget.propagate(at, action);
	_nvme_devices_widget.propagate(at, action);
	_mmc_devices_widget .propagate(at, action);
	_usb_devices_widget .propagate(at, action);

	_remove .propagate(at);
	_restart.propagate(at);
}


void Graph::clack(Clacked_at const &at, Action &action, Ram_fs_widget::Action &ram_fs_action)
{
	_ram_fs_widget      .propagate(at, ram_fs_action);
	_ahci_devices_widget.propagate(at, action);
	_nvme_devices_widget.propagate(at, action);
	_mmc_devices_widget .propagate(at, action);
	_usb_devices_widget .propagate(at, action);

	_remove.propagate(at, [&] {
		action.remove_deployed_component(_runtime_state.selected());

		/*
		 * Unselect the removed component to bring graph into
		 * default state.
		 */
		_runtime_state.toggle_selection(_runtime_state.selected(),
		                                _runtime_config);
	});

	_restart.propagate(at, [&] {
		action.restart_deployed_component(_runtime_state.selected());
	});
}

