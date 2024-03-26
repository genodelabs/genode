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

	template <typename FN>
	void with_query_result(FN const &fn) const
	{
		if (query_result.constructed())
			fn(query_result->xml());
	}

	template <typename FN>
	void with_entry_at_index(unsigned const index, FN const &fn) const
	{
		unsigned count = 0;
		with_query_result([&] (Xml_node node) {
			node.with_optional_sub_node("dir", [&] (Xml_node listing) {
				listing.for_each_sub_node([&] (Xml_node entry) {
					if (count++ == index)
						fn(entry); }); }); });
	}

	bool any_browsed_fs() const { return browsed_fs.length() > 0; }

	void gen_start_nodes(Xml_generator &xml) const
	{
		if (!fs_query.constructed() || !any_browsed_fs())
			return;

		xml.node("start", [&] {
			fs_query->gen_start_node_content(xml);

			gen_named_node(xml, "binary", "fs_query");

			xml.node("config", [&] {
				xml.node("vfs", [&] {
					xml.node("fs", [&] {}); });

				xml.node("query", [&] {
					xml.attribute("path", path);
				});
			});

			xml.node("route", [&] {
				gen_parent_rom_route(xml, "fs_query");
				gen_parent_rom_route(xml, "ld.lib.so");
				gen_parent_rom_route(xml, "vfs.lib.so");

				gen_parent_route<Cpu_session>     (xml);
				gen_parent_route<Pd_session>      (xml);
				gen_parent_route<Log_session>     (xml);
				gen_parent_route<Report::Session> (xml);

				gen_service_node<::File_system::Session>(xml, [&] {

					if (browsed_fs == "config") {
						xml.node("parent", [&] {
							xml.attribute("label", "config"); });
					}
					else if (browsed_fs == "report") {
						xml.node("parent", [&] {
							xml.attribute("label", "report"); });
					}
					else {
						xml.node("child", [&] {
							xml.attribute("name", browsed_fs); });
					}
				});
			});
		});

		if (edited_file.length() <= 1 || !text_area.constructed())
			return;

		xml.node("start", [&] {
			xml.attribute("name", text_area->name());

			text_area->gen_start_node_version(xml);

			xml.attribute("caps", 350);
			gen_named_node(xml, "resource", "RAM", [&] {
				xml.attribute("quantum", String<64>(22*1024*1024UL)); });

			gen_named_node(xml, "binary", "text_area");

			xml.node("config", [&] {
				Path const file_path = (path == "/")
				                     ? Path("/", edited_file)
				                     : Path(path, "/", edited_file);
				xml.attribute("path", file_path);
				xml.attribute("max_lines", 40);
				xml.attribute("min_width", 600);
				xml.attribute("copy", "yes");

				if (edit)
					xml.attribute("paste", "yes");
				else
					xml.attribute("watch", "yes");

				if (edit) {
					xml.node("save", [&] {
						xml.attribute("version", save_version); });

					xml.node("report", [&] {
						xml.attribute("saved", "yes"); });
				}

				xml.node("vfs", [&] {
					xml.node("fs", [&] {}); });
			});

			xml.node("route", [&] {
				gen_parent_rom_route(xml, "text_area");
				gen_parent_rom_route(xml, "ld.lib.so");
				gen_parent_rom_route(xml, "vfs.lib.so");
				gen_parent_rom_route(xml, "sandbox.lib.so");
				gen_parent_rom_route(xml, "menu_view");
				gen_parent_rom_route(xml, "libc.lib.so");
				gen_parent_rom_route(xml, "libm.lib.so");
				gen_parent_rom_route(xml, "libpng.lib.so");
				gen_parent_rom_route(xml, "zlib.lib.so");
				gen_parent_rom_route(xml, "menu_view_styles.tar");

				gen_parent_route<Cpu_session>     (xml);
				gen_parent_route<Pd_session>      (xml);
				gen_parent_route<Log_session>     (xml);
				gen_parent_route<Report::Session> (xml);
				gen_parent_route<Timer::Session>  (xml);

				gen_service_node<Rom_session>(xml, [&] {
					xml.attribute("label", "clipboard");
					xml.node("parent", [&] { }); });

				gen_service_node<Gui::Session>(xml, [&] {
					xml.node("parent", [&] {
						xml.attribute("label", "leitzentrale -> editor"); }); });

				gen_service_node<::File_system::Session>(xml, [&] {
					xml.attribute("label", "fonts");
					xml.node("parent", [&] {
						xml.attribute("label", "leitzentrale -> fonts"); }); });

				gen_service_node<::File_system::Session>(xml, [&] {

					if (browsed_fs == "config") {
						xml.node("parent", [&] {
							xml.attribute("label", "config"); });
					}
					else if (browsed_fs == "report") {
						xml.node("parent", [&] {
							xml.attribute("label", "report"); });
					}
					else {
						xml.node("child", [&] {
							xml.attribute("name", browsed_fs); });
					}
				});
			});
		});
	}
};

#endif /* _MODEL__FILE_BROWSER_STATE_H_ */
