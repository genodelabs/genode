/*
 * \brief  Configuration for the depot-query tool
 * \author Norman Feske
 * \date   2017-12-08
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include "node.h"

void Depot_download_manager::gen_depot_query_start_content(Generator &g,
                                                           Node const &installation,
                                                           Archive::User const &next_user,
                                                           Depot_query_version version,
                                                           List_model<Job> const &jobs)
{
	gen_common_start_content(g, "depot_query",
	                         Cap_quota{100}, Ram_quota{2*1024*1024});

	g.node("config", [&] {
		g.attribute("version", version.value);
		using Arch = String<32>;
		g.attribute("arch", installation.attribute_value("arch", Arch()));
		g.node("vfs", [&] {
			g.node("dir", [&] {
				g.attribute("name", "depot");
				g.node("fs", [&] {
					g.attribute("label", "depot -> /"); });
			});
		});

		/*
		 * Filter out failed parts of the installation from being re-queried.
		 * The inclusion of those parts may otherwise result in an infinite
		 * loop if the installation is downloaded from a mix of depot users.
		 */
		auto job_failed = [&] (Node const &node)
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
			installation.for_each_sub_node(node_type, [&] (Node const &node) {
				if (!job_failed(node))
					fn(node); });
		};

		auto propagate_verify_attr = [&] (Generator &g, Node const &node)
		{
			if (node.attribute_value("verify", true) == false)
				g.attribute("require_verify", "no");
		};

		for_each_install_sub_node("archive", [&] (Node const &archive) {
			g.node("dependencies", [&] {
				g.attribute("path", archive.attribute_value("path", Archive::Path()));
				g.attribute("source", archive.attribute_value("source", true));
				g.attribute("binary", archive.attribute_value("binary", true));
				propagate_verify_attr(g, archive);
			});
		});

		for_each_install_sub_node("index", [&] (Node const &index) {
			Archive::Path const path = index.attribute_value("path", Archive::Path());
			if (!Archive::index(path)) {
				warning("malformed index path '", path, "'");
				return;
			}
			g.node("index", [&] {
				g.attribute("user",    Archive::user(path));
				g.attribute("version", Archive::_path_element<Archive::Version>(path, 2));
				propagate_verify_attr(g, index);
			});
		});

		for_each_install_sub_node("image", [&] (Node const &image) {
			Archive::Path const path = image.attribute_value("path", Archive::Path());
			if (!Archive::image(path)) {
				warning("malformed image path '", path, "'");
				return;
			}
			g.node("image", [&] {
				g.attribute("user", Archive::user(path));
				g.attribute("name", Archive::name(path));
				propagate_verify_attr(g, image);
			});
		});

		for_each_install_sub_node("image_index", [&] (Node const &image_index) {
			Archive::Path const path = image_index.attribute_value("path", Archive::Path());
			if (!Archive::index(path) && Archive::name(path) != "index") {
				warning("malformed image-index path '", path, "'");
				return;
			}
			g.node("image_index", [&] {
				g.attribute("user", Archive::user(path));
				propagate_verify_attr(g, image_index);
			});
		});

		if (next_user.valid())
			g.node("user", [&] { g.attribute("name", next_user); });
	});

	g.node("route", [&] {
		g.node("service", [&] {
			g.attribute("name", File_system::Session::service_name());
			g.node("parent", [&] {
				g.attribute("identity", "depot"); });
		});
		gen_parent_unscoped_rom_route(g, "depot_query");
		gen_parent_unscoped_rom_route(g, "ld.lib.so");
		gen_parent_rom_route(g, "vfs.lib.so");
		gen_parent_route<Cpu_session>    (g);
		gen_parent_route<Pd_session>     (g);
		gen_parent_route<Log_session>    (g);
		gen_parent_route<Report::Session>(g);
	});
}
