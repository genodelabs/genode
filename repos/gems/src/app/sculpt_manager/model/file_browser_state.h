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
#include <model/service.h>

namespace Sculpt { struct File_browser_state; }

struct Sculpt::File_browser_state : Noncopyable
{
	using Name     = Start_name;
	using Resource = Start_name;

	struct Fs
	{
		Child_name    server;
		Service::Name resource;

		static Fs from_service(Service const &service)
		{
			if (service.type != Service::Type::FS)
				return { };

			return { .server  = service.server,
			        .resource = service.name };
		}

		bool operator == (Fs const &other) const
		{
			return server == other.server && resource == other.resource;
		}

		Child_name query_name() const
		{
			Child_name result = server;
			if (resource.length() > 1)
				result = { result, ".", resource };
			return { result, ".query" };
		}
	};

	using Path     = String<256>;
	using File     = Path;
	using Sub_dir  = Path;

	Fs browsed { };

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

	bool any_browsed_fs() const { return browsed.server.length() > 1; }

	void _connect_browsed_fs(Generator &g) const
	{
		g.node("fs", [&] {
			gen_named_node(g, "child", browsed.server, [&] {
				if (browsed.resource.length() > 1)
					g.attribute("identity", browsed.resource); }); });
	}

	void gen_child_nodes(Generator &g) const
	{
		if (!fs_query.constructed() || !any_browsed_fs())
			return;

		g.node("child", [&] {
			fs_query->gen_child_node_content(g);

			g.node("config", [&] {
				g.node("vfs", [&] {
					g.node("fs", [&] {}); });

				g.node("query", [&] {
					g.attribute("path", path);
				});
			});

			g.tabular_node("connect", [&] {
				connect_parent_rom(g, "vfs.lib.so");
				connect_report(g);
				_connect_browsed_fs(g);
			});
		});

		if (edited_file.length() <= 1 || !text_area.constructed())
			return;

		g.node("child", [&] {
			text_area->gen_child_node_content(g);

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

			g.tabular_node("connect", [&] {
				connect_parent_rom(g, "vfs.lib.so");
				connect_parent_rom(g, "sandbox.lib.so");
				connect_parent_rom(g, "dialog.lib.so");
				connect_parent_rom(g, "libc.lib.so");
				connect_parent_rom(g, "libm.lib.so");
				connect_parent_rom(g, "libpng.lib.so");
				connect_parent_rom(g, "zlib.lib.so");
				connect_parent_rom(g, "menu_view");
				connect_parent_rom(g, "menu_view_style.tar");

				gen_named_node(g, "report", "clipboard", [&] {
					gen_named_node(g, "child", "clipboard", [&] {
						g.attribute("label", "leitzentrale -> clipboard"); }); });

				connect_report(g);

				gen_named_node(g, "rom", "clipboard", [&] {
					gen_named_node(g, "child", "clipboard", [&] {
						g.attribute("label", "leitzentrale -> clipboard"); }); });

				g.node("gui", [&] {
					gen_named_node(g, "child", "leitzentrale", [&] {
						g.attribute("label", "editor"); }); });

				gen_named_node(g, "fs", "font", [&] {
					gen_named_node(g, "child", "font"); });

				_connect_browsed_fs(g);
			});
		});
	}
};

#endif /* _MODEL__FILE_BROWSER_STATE_H_ */
