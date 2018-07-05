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

namespace Sculpt { struct Graph; }


struct Sculpt::Graph
{
	Env &_env;

	Storage_target const &_sculpt_partition;

	Expanding_reporter _graph_dialog_reporter { _env, "dialog", "runtime_view_dialog" };

	/*
	 * Even though the runtime configuration is generate by the sculpt
	 * manager, we still obtain it as a separate ROM session to keep both
	 * parts decoupled.
	 */
	Attached_rom_dataspace _runtime_config_rom { _env, "config -> managed/runtime" };

	Signal_handler<Graph> _runtime_config_handler {
		_env.ep(), *this, &Graph::_handle_runtime_config };

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

	void _handle_runtime_config()
	{
		_runtime_config_rom.update();

		Xml_node const config = _runtime_config_rom.xml();

		_graph_dialog_reporter.generate([&] (Xml_generator &xml) {

			xml.node("depgraph", [&] () {

				config.for_each_sub_node("start", [&] (Xml_node start) {

					Start_name const name = start.attribute_value("name", Start_name());

					gen_named_node(xml, "frame", name, [&] () {

						Node_name primary_dep = _primary_dependency(start);

						if (primary_dep == "default_fs_rw")
							primary_dep = _sculpt_partition.fs();

						if (primary_dep.valid())
							xml.attribute("dep", primary_dep);

						gen_named_node(xml, "button", name, [&] () {
							xml.node("label", [&] () {
								xml.attribute("text", name);
							});
						});
					});
				});

				config.for_each_sub_node("start", [&] (Xml_node start) {

					Start_name const name = start.attribute_value("name", Start_name());

					bool const show_details = false;

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

	Graph(Env &env, Storage_target const &sculpt_partition)
	:
		_env(env), _sculpt_partition(sculpt_partition)
	{
		_runtime_config_rom.sigh(_runtime_config_handler);
	}
};

#endif /* _GRAPH_H_ */
