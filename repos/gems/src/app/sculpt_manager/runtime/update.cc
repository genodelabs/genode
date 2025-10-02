/*
 * \brief  Configuration for the depot-download subsystem
 * \author Norman Feske
 * \date   2018-05-08
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <runtime.h>

void Sculpt::gen_update_start_content(Generator &g)
{
	gen_common_start_content(g, "update",
	                         Cap_quota{2000}, Ram_quota{64*1024*1024},
	                         Priority::STORAGE);

	gen_named_node(g, "binary", "init");

	g.tabular_node("route", [&] {

		using Label = String<32>;
		auto gen_fs = [&] (Label const &label_prefix, Label const &server) {
			gen_service_node<::File_system::Session>(g, [&] {
				g.attribute("label_prefix", label_prefix);
				gen_named_node(g, "child", server); }); };

		/* connect file-system sessions to chroot instances */
		gen_fs("depot ->",  "depot_rw");
		gen_fs("public ->", "public_rw");

		gen_parent_rom_route(g, "ld.lib.so");
		gen_parent_rom_route(g, "vfs.lib.so");
		gen_parent_rom_route(g, "libc.lib.so");
		gen_parent_rom_route(g, "libm.lib.so");
		gen_parent_rom_route(g, "extract");
		gen_parent_rom_route(g, "verify");
		gen_parent_rom_route(g, "fetchurl");
		gen_parent_rom_route(g, "chroot");
		gen_parent_rom_route(g, "curl.lib.so");
		gen_parent_rom_route(g, "init");
		gen_parent_rom_route(g, "depot_query");
		gen_parent_rom_route(g, "depot_download_manager");
		gen_parent_rom_route(g, "report_rom");
		gen_parent_rom_route(g, "vfs");
		gen_parent_rom_route(g, "lxip.lib.so");
		gen_parent_rom_route(g, "vfs_lxip.lib.so");
		gen_parent_rom_route(g, "vfs_pipe.lib.so");
		gen_parent_rom_route(g, "posix.lib.so");
		gen_parent_rom_route(g, "libssh.lib.so");
		gen_parent_rom_route(g, "libssl.lib.so");
		gen_parent_rom_route(g, "libcrypto.lib.so");
		gen_parent_rom_route(g, "zlib.lib.so");
		gen_parent_rom_route(g, "libarchive.lib.so");
		gen_parent_rom_route(g, "liblzma.lib.so");
		gen_parent_rom_route(g, "config",       "depot_download.config");
		gen_parent_rom_route(g, "installation", "config -> managed/installation");
		gen_parent_route<Cpu_session>    (g);
		gen_parent_route<Pd_session>     (g);
		gen_parent_route<Rm_session>     (g);
		gen_parent_route<Timer::Session> (g);
		gen_parent_route<Report::Session>(g);

		auto gen_relabeled_log = [&] (Label const &label, Label const &relabeled) {
			gen_service_node<Log_session>(g, [&] {
				g.attribute("label", label);
				g.node("parent", [&] {
					g.attribute("label", relabeled); }); }); };

		/* shorten LOG-session labels to reduce the debug-output noise */
		gen_relabeled_log("dynamic -> fetchurl", "fetchurl");
		gen_relabeled_log("dynamic -> verify",   "verify");
		gen_relabeled_log("dynamic -> extract",  "extract");
		gen_parent_route<Log_session>(g);

		gen_service_node<Nic::Session>(g, [&] {
			gen_named_node(g, "child", "nic_router"); });
	});
}
