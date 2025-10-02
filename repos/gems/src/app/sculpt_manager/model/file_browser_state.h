/*
 * \brief  File_browser state
 * \author Norman Feske
 * \date   2020-01-31
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MODEL__FILE_BROWSER_STATE_H_
#define _MODEL__FILE_BROWSER_STATE_H_

#include "types.h"
#include <model/child_state.h>

namespace Sculpt { struct File_browser_state; }

struct Sculpt::File_browser_state : Noncopyable
{
	using Fs_name = Start_name;
	using Path    = String<256>;
	using File    = Path;
	using Sub_dir = Path;

	Fs_name browsed_fs { };

	Constructible<Child_state> fs_query { };
	Constructible<Child_state> text_area { };

	Constructible<Attached_rom_dataspace> query_result { };

	File_browser_state() { };

	Path path { };

	/*
	 * File viewing and editing
	 */

	File edited_file { };

	bool     edit         = false;
	bool     modified     = false;    /* edited file has unsaved modifications */
	unsigned save_version = 0;        /* version used for next save request */
	unsigned last_saved_version = 0;  /* last version successfully saved */

	void update_query_results()
	{
		if (query_result.constructed())
			query_result->update();
	}

	void with_query_result(auto const &fn) const
	{
		if (query_result.constructed())
			fn(query_result->node());
	}

	void with_entry_at_index(unsigned const index, auto const &fn) const
	{
		unsigned count = 0;
		with_query_result([&] (Node const &node) {
			node.with_optional_sub_node("dir", [&] (Node const &listing) {
				listing.for_each_sub_node([&] (Node const &entry) {
					if (count++ == index)
						fn(entry); }); }); });
	}

	bool any_browsed_fs() const { return browsed_fs.length() > 0; }

	void gen_start_nodes(Generator &g) const
	{
		if (!fs_query.constructed() || !any_browsed_fs())
			return;

		g.node("start", [&] {
			fs_query->gen_start_node_content(g);

			gen_named_node(g, "binary", "fs_query");

			g.node("config", [&] {
				g.node("vfs", [&] {
					g.node("fs", [&] {}); });

				g.node("query", [&] {
					g.attribute("path", path);
				});
			});

			g.tabular_node("route", [&] {
				gen_parent_rom_route(g, "fs_query");
				gen_parent_rom_route(g, "ld.lib.so");
				gen_parent_rom_route(g, "vfs.lib.so");

				gen_parent_route<Cpu_session>     (g);
				gen_parent_route<Pd_session>      (g);
				gen_parent_route<Log_session>     (g);
				gen_parent_route<Report::Session> (g);

				gen_service_node<::File_system::Session>(g, [&] {

					if (browsed_fs == "config") {
						g.node("parent", [&] {
							g.attribute("identity", "config"); });
					}
					else if (browsed_fs == "report") {
						g.node("parent", [&] {
							g.attribute("identity", "report"); });
					}
					else {
						g.node("child", [&] {
							g.attribute("name", browsed_fs); });
					}
				});
			});
		});

		if (edited_file.length() <= 1 || !text_area.constructed())
			return;

		g.node("start", [&] {
			text_area->gen_start_node_content(g);

			gen_named_node(g, "binary", "text_area");

			g.node("config", [&] {
				Path const file_path = (path == "/")
				                     ? Path("/", edited_file)
				                     : Path(path, "/", edited_file);
				g.attribute("path", file_path);
				g.attribute("max_lines", 40);
				g.attribute("min_width", 600);
				g.attribute("copy", "yes");

				if (edit)
					g.attribute("paste", "yes");
				else
					g.attribute("watch", "yes");

				if (edit) {
					g.node("save", [&] {
						g.attribute("version", save_version); });

					g.node("report", [&] {
						g.attribute("saved", "yes"); });
				}

				g.node("vfs", [&] {
					g.node("fs", [&] {}); });
			});

			g.tabular_node("route", [&] {
				gen_parent_rom_route(g, "text_area");
				gen_parent_rom_route(g, "ld.lib.so");
				gen_parent_rom_route(g, "vfs.lib.so");
				gen_parent_rom_route(g, "sandbox.lib.so");
				gen_parent_rom_route(g, "dialog.lib.so");
				gen_parent_rom_route(g, "menu_view");
				gen_parent_rom_route(g, "libc.lib.so");
				gen_parent_rom_route(g, "libm.lib.so");
				gen_parent_rom_route(g, "libpng.lib.so");
				gen_parent_rom_route(g, "zlib.lib.so");
				gen_parent_rom_route(g, "menu_view_styles.tar");

				gen_parent_route<Cpu_session>     (g);
				gen_parent_route<Pd_session>      (g);
				gen_parent_route<Log_session>     (g);
				gen_parent_route<Report::Session> (g);
				gen_parent_route<Timer::Session>  (g);

				gen_service_node<Rom_session>(g, [&] {
					g.attribute("label", "clipboard");
					g.node("parent", [&] { }); });

				gen_service_node<Gui::Session>(g, [&] {
					g.node("parent", [&] {
						g.attribute("label", "leitzentrale -> editor"); }); });

				gen_service_node<::File_system::Session>(g, [&] {
					g.attribute("label_prefix", "fonts ->");
					g.node("parent", [&] {
						g.attribute("identity", "leitzentrale -> fonts"); }); });

				gen_service_node<::File_system::Session>(g, [&] {

					if (browsed_fs == "config") {
						g.node("parent", [&] {
							g.attribute("identity", "config"); });
					}
					else if (browsed_fs == "report") {
						g.node("parent", [&] {
							g.attribute("identity", "report"); });
					}
					else {
						g.node("child", [&] {
							g.attribute("name", browsed_fs); });
					}
				});
			});
		});
	}
};

#endif /* _MODEL__FILE_BROWSER_STATE_H_ */
