/*
 * \brief  Configuration for the chroot component
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

void Depot_download_manager::gen_chroot_start_content(Generator &g,
                                                      Archive::User const &user)
{
	gen_common_start_content(g, Path("/depot/", user),
	                         Cap_quota{100}, Ram_quota{2*1024*1024});

	g.node("binary", [&] { g.attribute("name", "chroot"); });

	g.node("config", [&] {
		g.node("default-policy", [&] {
			g.attribute("path", Path("/", user));
			g.attribute("writeable", "yes"); }); });

	g.node("provides", [&] {
		g.node("service", [&] {
			g.attribute("name", File_system::Session::service_name()); }); });

	g.node("route", [&] {
		g.node("service", [&] {
			g.attribute("name", File_system::Session::service_name());
			g.node("parent", [&] {
				g.attribute("identity", "depot_rw"); });
		});
		gen_parent_unscoped_rom_route(g, "chroot");
		gen_parent_unscoped_rom_route(g, "ld.lib.so");
		gen_parent_route<Cpu_session>(g);
		gen_parent_route<Pd_session> (g);
		gen_parent_route<Log_session>(g);
	});
}
