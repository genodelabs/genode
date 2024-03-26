/*
 * \brief  XML configuration for invoking the gpt_write_tool
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

	void _gen_gpt_write_start_content(Xml_generator &, Storage_device const &,
	                                  Start_name const &, auto const &);
}


void Sculpt::_gen_gpt_write_start_content(Xml_generator        &xml,
                                          Storage_device const &device,
                                          Start_name     const &name,
                                          auto           const &gen_actions_fn)
{
	gen_common_start_content(xml, name, Cap_quota{100}, Ram_quota{2*1024*1024},
	                         Priority::STORAGE);

	gen_named_node(xml, "binary", "gpt_write");

	xml.node("config", [&] {
		xml.attribute("verbose",         "yes");
		xml.attribute("update_geometry", "yes");
		xml.attribute("preserve_hybrid", "yes");

		xml.node("actions", [&] { gen_actions_fn(xml); });
	});

	xml.node("route", [&] {

		Storage_target const target { device.label, device.port, Partition::Number { } };
		target.gen_block_session_route(xml);

		gen_parent_rom_route(xml, "gpt_write");
		gen_parent_rom_route(xml, "ld.lib.so");
		gen_parent_route<Cpu_session>    (xml);
		gen_parent_route<Pd_session>     (xml);
		gen_parent_route<Log_session>    (xml);
		gen_parent_route<Rom_session>    (xml);
	});
}


void Sculpt::gen_gpt_relabel_start_content(Xml_generator        &xml,
                                           Storage_device const &device)
{
	Start_name const name = device.relabel_start_name();
	_gen_gpt_write_start_content(xml, device, name, [&] (Xml_generator &xml) {

		device.for_each_partition([&] (Partition const &partition) {

			if (partition.number.valid() && partition.relabel_in_progress())
				xml.node("modify", [&] {
					xml.attribute("entry",     partition.number);
					xml.attribute("new_label", partition.next_label); }); }); });
}


void Sculpt::gen_gpt_expand_start_content(Xml_generator        &xml,
                                          Storage_device const &device)
{
	Start_name const name = device.expand_start_name();
	_gen_gpt_write_start_content(xml, device, name, [&] (Xml_generator &xml) {
		device.for_each_partition([&] (Partition const &partition) {

			if (partition.number.valid() && partition.gpt_expand_in_progress)
				xml.node("modify", [&] {
					xml.attribute("entry",    partition.number);
					xml.attribute("new_size", "max"); }); }); });
}
