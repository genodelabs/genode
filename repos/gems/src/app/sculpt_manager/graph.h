/*
 * \brief  Graph view of runtime state
 * \author Norman Feske
 * \date   2018-07-05
 *
 * The GUI is based on a dynamically configured init component, which hosts
 * one menu-view component for each dialog.
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _GRAPH_H_
#define _GRAPH_H_

/* Genode includes */
#include <os/reporter.h>

/* local includes */
#include <types.h>
#include <xml.h>
#include <model/capacity.h>
#include <model/popup.h>

namespace Sculpt { struct Graph; }


struct Sculpt::Graph
{
	Env &_env;

	Runtime_state &_runtime_state;

	Storage_target const &_sculpt_partition;

	Popup::State const &_popup_state;

	Depot_deploy::Children const &_deploy_children;

	Expanding_reporter _graph_dialog_reporter { _env, "dialog", "runtime_view_dialog" };

	/*
	 * Even though the runtime configuration is generate by the sculpt
	 * manager, we still obtain it as a separate ROM session to keep both
	 * parts decoupled.
	 */
	Attached_rom_dataspace _runtime_config_rom { _env, "config -> managed/runtime" };

	Attached_rom_dataspace _hover_rom { _env, "runtime_view_hover" };

	Signal_handler<Graph> _runtime_config_handler {
		_env.ep(), *this, &Graph::_handle_runtime_config };

	Signal_handler<Graph> _hover_handler {
		_env.ep(), *this, &Graph::_handle_hover };

	Hoverable_item   _node_button_item { };
	Hoverable_item   _add_button_item { };
	Activatable_item _remove_item   { };

	/*
	 * Defined when '+' button is hovered
	 */
	Rect _popup_anchor { };

	bool _hovered = false;

	typedef Start_name Node_name;

	/**
	 * Return component name targeted by the first route of the start node
	 */
	static Node_name _primary_dependency(Xml_node const start)
	{
		if (!start.has_sub_node("route"))
			return Node_name();

		Xml_node const route = start.sub_node("route");

		if (!route.has_sub_node("service"))
			return Node_name();

		Xml_node const service = route.sub_node("service");

		if (service.has_sub_node("child")) {
			Xml_node const child = service.sub_node("child");
			return child.attribute_value("name", Node_name());
		}

		return Node_name();
	}

	template <typename FN>
	static void _for_each_secondary_dep(Xml_node const start, FN const &fn)
	{
		if (!start.has_sub_node("route"))
			return;

		Xml_node const route = start.sub_node("route");

		bool first_route = true;
		route.for_each_sub_node("service", [&] (Xml_node service) {

			if (!first_route && service.has_sub_node("child")) {
				Xml_node const child = service.sub_node("child");
				fn(child.attribute_value("name", Start_name()));
			}

			first_route = false;
		});
	}

	void _gen_selected_node_content(Xml_generator &xml, Start_name const &name,
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

		String<100> const
			ram (Capacity{info.assigned_ram - info.avail_ram}, " / ",
			     Capacity{info.assigned_ram}),
			caps(info.assigned_caps - info.avail_caps, " / ",
			     info.assigned_caps, " caps");

		gen_named_node(xml, "label", "ram", [&] () {
			xml.attribute("text", ram); });

		gen_named_node(xml, "label", "caps", [&] () {
			xml.attribute("text", caps); });
	}

	void _gen_graph_dialog()
	{
		Xml_node const config = _runtime_config_rom.xml();

		_graph_dialog_reporter.generate([&] (Xml_generator &xml) {

			xml.node("depgraph", [&] () {

				gen_named_node(xml, "button", "global+", [&] () {
					_add_button_item.gen_button_attr(xml, "global+");

					if (_popup_state == Popup::VISIBLE)
						xml.attribute("selected", "yes");

					xml.node("label", [&] () {
						xml.attribute("text", "+"); }); });

				config.for_each_sub_node("start", [&] (Xml_node start) {

					Start_name const name = start.attribute_value("name", Start_name());

					Runtime_state::Info const info = _runtime_state.info(name);

					gen_named_node(xml, "frame", name, [&] () {

						Node_name primary_dep = _primary_dependency(start);

						if (primary_dep == "default_fs_rw")
							primary_dep = _sculpt_partition.fs();

						if (primary_dep.valid())
							xml.attribute("dep", primary_dep);

						xml.node("vbox", [&] () {

							gen_named_node(xml, "button", name, [&] () {
								_node_button_item.gen_button_attr(xml, name);

								if (info.selected)
									xml.attribute("selected", "yes");

								xml.node("label", [&] () {
									xml.attribute("text", name);
								});
							});

							if (info.selected)
								_gen_selected_node_content(xml, name, info);
						});
					});
				});

				config.for_each_sub_node("start", [&] (Xml_node start) {

					Start_name const name = start.attribute_value("name", Start_name());

					Runtime_state::Info const info = _runtime_state.info(name);

					bool const show_details = info.selected;

					if (show_details) {
						_for_each_secondary_dep(start, [&] (Start_name const &dep) {
							xml.node("dep", [&] () {
								xml.attribute("node", name);
								xml.attribute("on", dep);
							});
						});
					}
				});
			});
		});
	}

	void _handle_runtime_config()
	{
		_runtime_config_rom.update();

		_gen_graph_dialog();
	}

	void _handle_hover()
	{
		_hover_rom.update();

		Xml_node const hover = _hover_rom.xml();

		_hovered = (hover.num_sub_nodes() != 0);

		bool const changed =
			_node_button_item.match(hover, "dialog", "depgraph", "frame", "vbox", "button", "name") |
			_add_button_item .match(hover, "dialog", "depgraph", "button", "name") |
			_remove_item     .match(hover, "dialog", "depgraph", "frame", "vbox",
			                               "frame", "vbox", "button", "name");

		if (_add_button_item.hovered("global+")) {

			/* update anchor geometry of popup menu */
			auto hovered_rect = [] (Xml_node const hover) {

				auto point_from_xml = [] (Xml_node node) {
					return Point(node.attribute_value("xpos", 0L),
					             node.attribute_value("ypos", 0L)); };

				auto area_from_xml = [] (Xml_node node) {
					return Area(node.attribute_value("width",  0UL),
					            node.attribute_value("height", 0UL)); };

				if (!hover.has_sub_node("dialog")) return Rect();
				Xml_node const dialog = hover.sub_node("dialog");

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

		if (changed)
			_gen_graph_dialog();
	}

	Graph(Env                          &env,
	      Runtime_state                &runtime_state,
	      Storage_target         const &sculpt_partition,
	      Popup::State           const &popup_state,
	      Depot_deploy::Children const &deploy_children)
	:
		_env(env), _runtime_state(runtime_state),
		_sculpt_partition(sculpt_partition),
		_popup_state(popup_state), _deploy_children(deploy_children)
	{
		_runtime_config_rom.sigh(_runtime_config_handler);
		_hover_rom.sigh(_hover_handler);
	}

	bool hovered() const { return _hovered; }

	bool add_button_hovered() const { return _add_button_item._hovered.valid(); }

	struct Action : Interface
	{
		virtual void remove_deployed_component(Start_name const &) = 0;
		virtual void toggle_launcher_selector(Rect) = 0;
	};

	void click(Action &action)
	{
		if (_add_button_item._hovered.valid()) {
			action.toggle_launcher_selector(_popup_anchor);
		}

		if (_node_button_item._hovered.valid()) {
			_runtime_state.toggle_selection(_node_button_item._hovered);
			_remove_item.reset();
			_gen_graph_dialog();
		}

		if (_remove_item.hovered("remove")) {
			_remove_item.propose_activation_on_click();
			_gen_graph_dialog();
		}
	}

	void clack(Action &action)
	{
		if (_remove_item.hovered("remove")) {

			_remove_item.confirm_activation_on_clack();

			if (_remove_item.activated("remove"))
				action.remove_deployed_component(_runtime_state.selected());

		} else {
			_remove_item.reset();
		}

		_gen_graph_dialog();
	}
};

#endif /* _GRAPH_H_ */
