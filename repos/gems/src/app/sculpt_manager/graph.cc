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
#include <view/dialog.h>

using namespace Sculpt;


void Graph::_gen_selected_node_content(Xml_generator &xml, Start_name const &name,
                                       Runtime_state::Info const &info) const
{
	bool const removable = _deploy_children.exists(name);

	if (removable) {
		gen_named_node(xml, "frame", "operations", [&] () {
			xml.node("vbox", [&] () {
				gen_named_node(xml, "button", "remove", [&] () {
					_remove_item.gen_button_attr(xml, "remove");
					xml.node("label", [&] () {
						xml.attribute("text", "Remove");
					});
				});
			});
		});
	}

	if (name == "ram_fs")
		gen_named_node(xml, "frame", "ram_fs_operations", [&] () {
			xml.node("vbox", [&] () {
				_ram_fs_dialog.generate(xml, _ram_fs_state); }); });

	String<100> const
		ram (Capacity{info.assigned_ram - info.avail_ram}, " / ",
		     Capacity{info.assigned_ram}),
		caps(info.assigned_caps - info.avail_caps, " / ",
		     info.assigned_caps, " caps");

	gen_named_node(xml, "label", "hspace", [&] () {
		xml.attribute("min_ex", 25); });

	gen_named_node(xml, "label", "ram", [&] () {
		xml.attribute("text", ram); });

	gen_named_node(xml, "label", "caps", [&] () {
		xml.attribute("text", caps); });
}


void Graph::_gen_parent_node(Xml_generator &xml, Start_name const &name,
                             Label const &label) const
{
	gen_named_node(xml, "frame", name, [&] () {
		xml.node("label", [&] () {
			xml.attribute("text", Start_name(" ", label, " ")); }); });
}


void Graph::_gen_storage_node(Xml_generator &xml) const
{
	char const * const name = "storage";

	bool const any_selected = _runtime_state.selected().valid();
	bool const unimportant  = any_selected && !_runtime_state.storage_in_tcb();

	gen_named_node(xml, "frame", name, [&] () {

		if (unimportant)
			xml.attribute("style", "unimportant");

		xml.node("vbox", [&] () {

			gen_named_node(xml, "button", name, [&] () {

				_node_button_item.gen_button_attr(xml, name);

				if (unimportant)
					xml.attribute("style", "unimportant");

				if (_storage_selected)
					xml.attribute("selected", "yes");

				xml.node("label", [&] () { xml.attribute("text", "Storage"); });
			});

			if (_storage_selected)
				gen_named_node(xml, "frame", "storage_operations", [&] () {
					xml.node("vbox", [&] () {
						_storage_dialog->gen_block_devices(xml); }); });
		});
	});
}


void Graph::_gen_usb_node(Xml_generator &xml) const
{
	char const * const name = "usb";

	bool const any_selected = _runtime_state.selected().valid();
	bool const unimportant  = any_selected && !_runtime_state.usb_in_tcb();

	gen_named_node(xml, "frame", name, [&] () {

		if (unimportant)
			xml.attribute("style", "unimportant");

		xml.node("vbox", [&] () {

			gen_named_node(xml, "button", name, [&] () {

				_node_button_item.gen_button_attr(xml, name);

				if (unimportant)
					xml.attribute("style", "unimportant");

				if (_usb_selected)
					xml.attribute("selected", "yes");

				xml.node("label", [&] () { xml.attribute("text", "USB"); });
			});

			if (_usb_selected)
				gen_named_node(xml, "frame", "usb_operations", [&] () {
					xml.node("vbox", [&] () {
						_storage_dialog->gen_usb_storage_devices(xml); }); });
		});
	});
}


void Graph::generate(Xml_generator &xml) const
{
	xml.node("depgraph", [&] () {

		if (_sculpt_partition.valid()) {
			gen_named_node(xml, "button", "global+", [&] () {
				_add_button_item.gen_button_attr(xml, "global+");

				if (_popup_state == Popup::VISIBLE)
					xml.attribute("selected", "yes");

				xml.node("label", [&] () {
					xml.attribute("text", "+"); }); });
		}

		_gen_storage_node(xml);

		if (_storage_devices.usb_present)
			_gen_usb_node(xml);
		else
			_gen_parent_node(xml, "usb", "USB");

		/* parent roles */
		_gen_parent_node(xml, "hardware", "Hardware");
		_gen_parent_node(xml, "config",   "Config");
		_gen_parent_node(xml, "info",     "Info");
		_gen_parent_node(xml, "GUI",      "GUI");

		typedef Runtime_config::Component Component;

		bool const any_selected = _runtime_state.selected().valid();

		_runtime_config.for_each_component([&] (Component const &component) {

			Start_name const name = component.name;
			Start_name const pretty_name { Pretty(name) };

			/* omit sculpt's helpers from the graph */
			bool const blacklisted = (name == "runtime_view"
			                       || name == "popup_view"
			                       || name == "menu_view"
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
			                       || name == "depot_query");
			if (blacklisted)
				return;

			Runtime_state::Info const info = _runtime_state.info(name);

			bool const unimportant = any_selected && !info.tcb;

			gen_named_node(xml, "frame", name, [&] () {

				if (unimportant)
					xml.attribute("style", "unimportant");

				Start_name primary_dep = component.primary_dependency;

				if (primary_dep == "default_fs_rw")
					primary_dep = _sculpt_partition.fs();

				if (primary_dep.valid()) {
					xml.attribute("dep", primary_dep);
					if (unimportant)
						xml.attribute("dep_visible", false);
				}

				xml.node("vbox", [&] () {

					gen_named_node(xml, "button", name, [&] () {

						if (unimportant)
							xml.attribute("style", "unimportant");

						_node_button_item.gen_button_attr(xml, name);

						if (info.selected)
							xml.attribute("selected", "yes");

						xml.node("label", [&] () {
							xml.attribute("text", pretty_name);
						});
					});

					if (info.selected)
						_gen_selected_node_content(xml, name, info);
				});
			});
		});

		_runtime_config.for_each_component([&] (Component const &component) {

			Start_name const name = component.name;

			Runtime_state::Info const info = _runtime_state.info(name);

			bool const show_details = info.tcb;

			if (show_details) {
				component.for_each_secondary_dep([&] (Start_name dep) {

					if (Runtime_state::blacklisted_from_graph(dep))
						return;

					if (dep == "default_fs_rw")
						dep = _sculpt_partition.fs();

					xml.node("dep", [&] () {
						xml.attribute("node", name);
						xml.attribute("on",   dep);
					});
				});
			}
		});
	});
}


Dialog::Hover_result Graph::hover(Xml_node hover)
{
	Hover_result const storage_dialog_hover_result =
		_storage_dialog->match_sub_dialog(hover, "depgraph", "frame", "vbox", "frame", "vbox");

	Dialog::Hover_result const hover_result = Dialog::any_hover_changed(
		storage_dialog_hover_result,
		_ram_fs_dialog.match_sub_dialog(hover, "depgraph", "frame", "vbox", "frame", "vbox"),
		_node_button_item.match(hover, "depgraph", "frame", "vbox", "button", "name"),
		_add_button_item .match(hover, "depgraph", "button", "name"),
		_remove_item     .match(hover, "depgraph", "frame", "vbox",
		                               "frame", "vbox", "button", "name"));

	if (_add_button_item.hovered("global+")) {

		/* update anchor geometry of popup menu */
		auto hovered_rect = [] (Xml_node const dialog)
		{
			if (!dialog.has_type("dialog")) return Rect();

			auto point_from_xml = [] (Xml_node node) {
				return Point(node.attribute_value("xpos", 0L),
				             node.attribute_value("ypos", 0L)); };

			auto area_from_xml = [] (Xml_node node) {
				return Area(node.attribute_value("width",  0UL),
				            node.attribute_value("height", 0UL)); };

			if (!dialog.has_sub_node("depgraph")) return Rect();
			Xml_node const depgraph = dialog.sub_node("depgraph");

			if (!depgraph.has_sub_node("button")) return Rect();
			Xml_node const button = depgraph.sub_node("button");

			return Rect(point_from_xml(dialog) + point_from_xml(depgraph) +
			            point_from_xml(button),
			            area_from_xml(button));
		};

		_popup_anchor = hovered_rect(hover);
	}

	return hover_result;
}


void Graph::click(Action &action)
{
	if (_ram_fs_dialog.click(action) == Click_result::CONSUMED)
		return;

	if (_storage_dialog_visible())
		if (_storage_dialog->click(action) == Click_result::CONSUMED)
			return;

	if (_add_button_item._hovered.valid())
		action.toggle_launcher_selector(_popup_anchor);

	if (_node_button_item._hovered.valid()) {

		_storage_selected = !_storage_selected && _node_button_item.hovered("storage");
		_usb_selected     = !_usb_selected     && _node_button_item.hovered("usb");

		/* reset storage dialog */
		if (_usb_selected || _storage_selected)
			_storage_dialog.construct(_storage_devices, _sculpt_partition);

		_runtime_state.toggle_selection(_node_button_item._hovered,
		                                _runtime_config);
		_remove_item.reset();
	}

	if (_remove_item.hovered("remove"))
		_remove_item.propose_activation_on_click();
}


void Graph::clack(Action &action, Ram_fs_dialog::Action &ram_fs_action)
{
	if (_ram_fs_dialog.clack(ram_fs_action) == Clack_result::CONSUMED)
		return;

	if (_storage_dialog_visible())
		if (_storage_dialog->clack(action) == Clack_result::CONSUMED)
			return;

	if (_remove_item.hovered("remove")) {

		_remove_item.confirm_activation_on_clack();

		if (_remove_item.activated("remove")) {
			action.remove_deployed_component(_runtime_state.selected());

			/*
			 * Unselect the removed component to bring graph into
			 * default state.
			 */
			_runtime_state.toggle_selection(_runtime_state.selected(),
			                                _runtime_config);
		}
	} else {
		_remove_item.reset();
	}
}

