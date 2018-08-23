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

namespace Sculpt { struct Graph; }


struct Sculpt::Graph
{
	Env &_env;

	Runtime_state &_runtime_state;

	Storage_target const &_sculpt_partition;

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

	Hoverable_item _node_button_item { };

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

			if (!service.has_sub_node("child"))
				return;

			if (!first_route) {
				Xml_node const child = service.sub_node("child");
				fn(child.attribute_value("name", Start_name()));
			}

			first_route = false;
		});
	}

	void _gen_graph_dialog()
	{
		Xml_node const config = _runtime_config_rom.xml();

		_graph_dialog_reporter.generate([&] (Xml_generator &xml) {

			xml.node("depgraph", [&] () {

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

							if (info.selected) {

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
			_node_button_item.match(hover, "dialog", "depgraph", "frame", "vbox", "button", "name");

		if (changed)
			_gen_graph_dialog();
	}

	Graph(Env &env, Runtime_state &runtime_state,
	      Storage_target const &sculpt_partition)
	:
		_env(env), _runtime_state(runtime_state), _sculpt_partition(sculpt_partition)
	{
		_runtime_config_rom.sigh(_runtime_config_handler);
		_hover_rom.sigh(_hover_handler);
	}

	bool hovered() const { return _hovered; }

	void click()
	{
		if (_node_button_item._hovered.valid()) {
			_runtime_state.toggle_selection(_node_button_item._hovered);
			_gen_graph_dialog();
		}
	}
};

#endif /* _GRAPH_H_ */
