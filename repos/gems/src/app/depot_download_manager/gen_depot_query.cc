/*
 * \brief  XML configuration for the depot-query tool
 * \author Norman Feske
 * \date   2017-12-08
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include "xml.h"

void Depot_download_manager::gen_depot_query_start_content(Xml_generator &xml,
                                                           Xml_node const &installation,
                                                           Archive::User const &next_user,
                                                           Depot_query_version version,
                                                           List_model<Job> const &jobs)
{
	gen_common_start_content(xml, "depot_query",
	                         Cap_quota{100}, Ram_quota{2*1024*1024});

	xml.node("config", [&] () {
		xml.attribute("version", version.value);
		using Arch = String<32>;
		xml.attribute("arch", installation.attribute_value("arch", Arch()));
		xml.node("vfs", [&] () {
			xml.node("dir", [&] () {
				xml.attribute("name", "depot");
				xml.node("fs", [&] () {
					xml.attribute("label", "depot -> /"); });
			});
		});

		/*
		 * Filter out failed parts of the installation from being re-queried.
		 * The inclusion of those parts may otherwise result in an infinite
		 * loop if the installation is downloaded from a mix of depot users.
		 */
		auto job_failed = [&] (Xml_node const &node)
		{
			Archive::Path const path = node.attribute_value("path", Archive::Path());

			bool failed = false;
			jobs.for_each([&] (Job const &job) {
				if (job.path == path && job.failed)
					failed = true; });

			return failed;
		};

		auto for_each_install_sub_node = [&] (auto node_type, auto const &fn)
		{
			installation.for_each_sub_node(node_type, [&] (Xml_node const &node) {
				if (!job_failed(node))
					fn(node); });
		};

		auto propagate_verify_attr = [&] (Xml_generator &xml, Xml_node const &node)
		{
			if (node.attribute_value("verify", true) == false)
				xml.attribute("require_verify", "no");
		};

		for_each_install_sub_node("archive", [&] (Xml_node const &archive) {
			xml.node("dependencies", [&] () {
				xml.attribute("path", archive.attribute_value("path", Archive::Path()));
				xml.attribute("source", archive.attribute_value("source", true));
				xml.attribute("binary", archive.attribute_value("binary", true));
				propagate_verify_attr(xml, archive);
			});
		});

		for_each_install_sub_node("index", [&] (Xml_node const &index) {
			Archive::Path const path = index.attribute_value("path", Archive::Path());
			if (!Archive::index(path)) {
				warning("malformed index path '", path, "'");
				return;
			}
			xml.node("index", [&] () {
				xml.attribute("user",    Archive::user(path));
				xml.attribute("version", Archive::_path_element<Archive::Version>(path, 2));
				propagate_verify_attr(xml, index);
			});
		});

		for_each_install_sub_node("image", [&] (Xml_node const &image) {
			Archive::Path const path = image.attribute_value("path", Archive::Path());
			if (!Archive::image(path)) {
				warning("malformed image path '", path, "'");
				return;
			}
			xml.node("image", [&] () {
				xml.attribute("user", Archive::user(path));
				xml.attribute("name", Archive::name(path));
				propagate_verify_attr(xml, image);
			});
		});

		for_each_install_sub_node("image_index", [&] (Xml_node const &image_index) {
			Archive::Path const path = image_index.attribute_value("path", Archive::Path());
			if (!Archive::index(path) && Archive::name(path) != "index") {
				warning("malformed image-index path '", path, "'");
				return;
			}
			xml.node("image_index", [&] () {
				xml.attribute("user", Archive::user(path));
				propagate_verify_attr(xml, image_index);
			});
		});

		if (next_user.valid())
			xml.node("user", [&] () { xml.attribute("name", next_user); });
	});

	xml.node("route", [&] () {
		xml.node("service", [&] () {
			xml.attribute("name", File_system::Session::service_name());
			xml.node("parent", [&] () {
				xml.attribute("identity", "depot"); });
		});
		gen_parent_unscoped_rom_route(xml, "depot_query");
		gen_parent_unscoped_rom_route(xml, "ld.lib.so");
		gen_parent_rom_route(xml, "vfs.lib.so");
		gen_parent_route<Cpu_session>    (xml);
		gen_parent_route<Pd_session>     (xml);
		gen_parent_route<Log_session>    (xml);
		gen_parent_route<Report::Session>(xml);
	});
}
