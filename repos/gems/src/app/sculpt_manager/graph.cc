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
		s.node("frame", [&] { s.sub_node("label", [&] { s.g.node("text", [&] {
			s.g.append_quoted(Sculpt::Start_name(" ", text, " ")); }); }); });
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
		bool alert;
		Dialog::Id primary_dep;
		Start_name pretty_name;
	};

	static void view(Scope<Depgraph> &s, Id const &id,
	                 Attr const &attr, auto const &selected_fn)
	{
		s.sub_scope<Frame>(id, [&] (Scope<Depgraph, Frame> &s) {

			if (attr.alert)
				s.attribute("style", "alert");
			else if (!attr.important)
				s.attribute("style", "unimportant");

			if (attr.primary_dep.valid()) {
				s.attribute("dep", attr.primary_dep.value);
				if (!attr.important)
					s.attribute("dep_visible", false);
			}

			s.sub_scope<Vbox>([&] (Scope<Depgraph, Frame, Vbox> &s) {

				s.sub_scope<Button>(id, [&] (Scope<Depgraph, Frame, Vbox, Button> &s) {

					bool const dimmed = (!attr.important || attr.alert)
					                  && !attr.selected;

					if (dimmed)        s.attribute("style",    "unimportant");
					if (s.hovered())   s.attribute("hovered",  "yes");
					if (attr.selected) s.attribute("selected", "yes");

					s.sub_scope<Label>(attr.pretty_name);
				});

				if (attr.selected)
					selected_fn(s);
			});
		});
	}
};


void Graph::_view_selected_node_content(Scope<Depgraph, Frame, Vbox> &s,
                                        Runtime_config::Component const &component,
                                        Attr const &attr) const
{
	s.sub_scope<Frame>([&] (Scope<Depgraph, Frame, Vbox, Frame> &s) {

		if (attr.alert) s.attribute("style", "alert");

		s.sub_scope<Vbox>([&] (Scope<Depgraph, Frame, Vbox, Frame, Vbox> &s) {

			if (component._stalled.constructed()) {
				s.sub_scope<Vgap>();
				component._stalled->with_optional_sub_node("deploy", [&] (Node const &deploy) {

					deploy.with_optional_sub_node("pkg_corrupt", [&] (Node const &) {
						auto const pkg = deploy.attribute_value("pkg", Depot::Archive::Path());
						s.sub_scope<Label>(String<80>(" corrupt ", pkg, " ")); });

					deploy.with_optional_sub_node("pkg_missing", [&] (Node const &) {
						auto const pkg = deploy.attribute_value("pkg", Depot::Archive::Path());
						s.sub_scope<Label>(String<80>(" missing ", pkg, " ")); });

					deploy.with_optional_sub_node("deps", [&] (Node const &dep) {
						dep.for_each_sub_node([&] (Node const &node) {
							Start_name const server = node.attribute_value("name", Start_name());
							node.for_each_sub_node([&] (Node const &resource) {
								auto detail = resource.attribute_value("name", String<32>());
								if (detail.length() > 1)
									detail = { " (", detail, ")" };
								s.sub_scope<Label>(String<80>(" requires ", server,
								                              " for ", resource.type(),
								                              detail, " "));
							});
						});
					});
				});
				s.sub_scope<Vgap>();
			}
			if (attr.ram.requested || attr.caps.requested) {
				String<128> msg { };
				if (attr.ram.requested) {
					msg = { msg, " RAM quota (", Num_bytes{attr.ram.requested}, ")" };
					if (attr.caps.requested)
						msg = { msg, "," };
				}
				if (attr.caps.requested)
					msg = { msg, " cap quota (", attr.caps.requested, ")" };
				msg = { msg, " requested " };

				s.sub_scope<Vgap>();
				s.sub_scope<Label>(msg);
				s.sub_scope<Vgap>();
			}
			s.sub_scope<Hbox>([&] (Scope<Depgraph, Frame, Vbox, Frame, Vbox, Hbox> &s) {

				if (attr.ram.requested || attr.caps.requested)
					s.widget(_grant);

				s.widget(_remove);
				s.widget(_restart);
			});
		});
	});

	if (component.name == "ram_fs")
		s.widget(_ram_fs_widget, _selected_target, _ram_fs_state);

	if (component.name == "intel_fb" || component.name == "vesa_fb")
		s.widget(_fb_widget, _fb_connectors, _fb_config, _hovered_display);

	String<100> const
		ram (Capacity{attr.ram.assigned - attr.ram.avail}, " / ",
		     Capacity{attr.ram.assigned}),
		caps(attr.caps.assigned - attr.caps.avail, " / ",
		     attr.caps.assigned, " caps");

	s.sub_scope<Min_ex>(25);
	s.sub_scope<Label>(ram);
	s.sub_scope<Label>(caps);

	if ((component.name == "usb") && _storage_devices.num_usb_devices)
		s.sub_scope<Frame>([&] (Scope<Depgraph, Frame, Vbox, Frame> &s) {
			s.widget(_usb_devices_widget); });

	if (component.name == "ahci")
		s.sub_scope<Frame>([&] (Scope<Depgraph, Frame, Vbox, Frame> &s) {
			s.widget(_ahci_devices_widget); });

	if (component.name == "nvme")
		s.sub_scope<Frame>([&] (Scope<Depgraph, Frame, Vbox, Frame> &s) {
			s.widget(_nvme_devices_widget); });

	if (component.name == "mmc")
		s.sub_scope<Frame>([&] (Scope<Depgraph, Frame, Vbox, Frame> &s) {
			s.widget(_mmc_devices_widget); });
}


void Graph::view(Scope<Depgraph> &s) const
{
	if (Feature::PRESENT_PLUS_MENU && _selected_target.valid())
		s.widget(_plus, _popup_state == Popup::VISIBLE);

	/* parent roles */
	s.sub_scope<Parent_node>(Id { "hardware" }, "Hardware");

	using Component = Runtime_config::Component;

	bool const any_selected = _runtime_config.selected().valid();

	_runtime_config.for_each_component([&] (Component const &component) {

		Start_name const name = component.name;
		Start_name pretty_name { Pretty(name) };

		if (name == "mmc-mmcblk0.part")
			pretty_name = "0.part";

		if (name == "mmc-mmcblk0.1.fs")
			pretty_name = "1.fs";

		/* omit sculpt's helpers from the graph */
		bool const hidden = (name == "runtime_view"
		                  || name == "editor"
		                  || name == "model_query"
		                  || name == "update"
		                  || name == "fs_tool"
		                  || name == "depot_rw"
		                  || name == "public_rw"
		                  || name == "depot_rom"
		                  || name == "dynamic_depot_rom"
		                  || name == "depot_query"
		                  || name == "blueprint_query"
		                  || name == "dir_query"
		                  || name == "manager_keyboard");
		if (hidden)
			return;

		bool const unimportant = any_selected && !component.tcb;

		/* basic categories, like GUI */
		Dialog::Id primary_dep = Id { component.primary_dependency };

		if (primary_dep.value == "default_fs_rw")
			primary_dep = Dialog::Id { _selected_target.fs() };

		/* primary dependency is another component */
		_runtime_config.with_graph_id(primary_dep,
			[&] (Dialog::Id const &id) { primary_dep = id; });

		Runtime_state::Info const info = _runtime_state.info(name);

		bool const alert = component._stalled.constructed()
		                || info.ram.requested || info.caps.requested;

		Selectable_node::view(s, component.graph_id,
			{
				.selected    = component.selected,
				.important   = !unimportant,
				.alert       = alert,
				.primary_dep = primary_dep,
				.pretty_name = pretty_name
			},
			[&] (Scope<Depgraph, Frame, Vbox> &s) {
				_view_selected_node_content(s, component, {
					.ram   = info.ram,
					.caps  = info.caps,
					.alert = alert
				});
			}
		);
	});

	_runtime_config.for_each_component([&] (Component const &component) {

		Start_name const name = component.name;

		if (name == "ram_fs")
			return;

		bool const show_details = component.tcb;

		if (show_details) {
			component.for_each_secondary_dep([&] (Start_name dep_name) {

				if (Runtime_config::blacklisted_from_graph(dep_name))
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
			_runtime_config.toggle_selection(name, _selected_target); });

	_plus.propagate(at, [&] {

		auto popup_anchor = [] (Node const &dialog)
		{
			Rect result { };
			dialog.with_optional_sub_node("depgraph", [&] (Node const &depgraph) {
				depgraph.with_optional_sub_node("button", [&] (Node const &button) {
					result = Rect(Point::from_node(dialog) + Point::from_node(depgraph) +
					              Point::from_node(button),
					              Area::from_node(button)); });
			});
			return result;
		};

		action.open_popup_dialog(popup_anchor(at._location));
	});

	_ram_fs_widget      .propagate(at, _selected_target, action);
	_fb_widget          .propagate(at, _fb_connectors,   action);
	_ahci_devices_widget.propagate(at, action);
	_nvme_devices_widget.propagate(at, action);
	_mmc_devices_widget .propagate(at, action);
	_usb_devices_widget .propagate(at, action);

	_grant.propagate(at, [&] {
		action.grant_resource_request(_runtime_config.selected()); });

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
		action.remove_deployed_component(_runtime_config.selected());

		/*
		 * Unselect the removed component to bring graph into
		 * default state.
		 */
		_runtime_config.toggle_selection(_runtime_config.selected(),
		                                 _selected_target);
	});

	_restart.propagate(at, [&] {
		action.restart_deployed_component(_runtime_config.selected());
	});
}

