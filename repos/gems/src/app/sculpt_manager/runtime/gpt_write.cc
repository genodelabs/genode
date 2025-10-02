/*
 * \brief  Configuration for invoking the gpt_write_tool
 * \author Norman Feske
 * \date   2018-05-16
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <runtime.h>

namespace Sculpt {

	void _gen_gpt_write_start_content(Generator &, Storage_device const &,
	                                  Start_name const &, auto const &);
}


void Sculpt::_gen_gpt_write_start_content(Generator            &g,
                                          Storage_device const &device,
                                          Start_name     const &name,
                                          auto           const &gen_actions_fn)
{
	gen_common_start_content(g, name, Cap_quota{100}, Ram_quota{2*1024*1024},
	                         Priority::STORAGE);

	gen_named_node(g, "binary", "gpt_write");

	g.node("config", [&] {
		g.attribute("verbose",         "yes");
		g.attribute("update_geometry", "yes");
		g.attribute("preserve_hybrid", "yes");

		g.node("actions", [&] { gen_actions_fn(g); });
	});

	g.tabular_node("route", [&] {

		Storage_target const target { device.driver, device.port, Partition::Number { } };
		target.gen_block_session_route(g);

		gen_parent_rom_route(g, "gpt_write");
		gen_parent_rom_route(g, "ld.lib.so");
		gen_parent_route<Cpu_session>    (g);
		gen_parent_route<Pd_session>     (g);
		gen_parent_route<Log_session>    (g);
		gen_parent_route<Rom_session>    (g);
	});
}


void Sculpt::gen_gpt_relabel_start_content(Generator            &g,
                                           Storage_device const &device)
{
	Start_name const name = device.relabel_start_name();
	_gen_gpt_write_start_content(g, device, name, [&] (Generator &g) {

		device.for_each_partition([&] (Partition const &partition) {

			if (partition.number.valid() && partition.relabel_in_progress())
				g.node("modify", [&] {
					g.attribute("entry",     partition.number);
					g.attribute("new_label", partition.next_label); }); }); });
}


void Sculpt::gen_gpt_expand_start_content(Generator            &g,
                                          Storage_device const &device)
{
	Start_name const name = device.expand_start_name();
	_gen_gpt_write_start_content(g, device, name, [&] (Generator &g) {
		device.for_each_partition([&] (Partition const &partition) {

			if (partition.number.valid() && partition.gpt_expand_in_progress)
				g.node("modify", [&] {
					g.attribute("entry",    partition.number);
					g.attribute("new_size", "max"); }); }); });
}
