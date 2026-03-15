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

void Sculpt::gen_update_child_content(Generator &g)
{
	gen_child_attr(g, Child_name { "update" }, Binary_name { "init" },
	               Cap_quota{2000}, Ram_quota{64*1024*1024}, Priority::STORAGE);

	g.tabular_node("connect", [&] {

		using Label = String<32>;
		auto gen_fs = [&] (Label const &name, Label const &server) {
			gen_named_node(g, "fs", name, [&] {
				gen_named_node(g, "child", server); }); };

		/* connect file-system sessions to chroot instances */
		gen_fs("depot",  "depot_rw");
		gen_fs("public", "public_rw");

		connect_parent_rom(g, "ld.lib.so");
		connect_parent_rom(g, "vfs.lib.so");
		connect_parent_rom(g, "libc.lib.so");
		connect_parent_rom(g, "libm.lib.so");
		connect_parent_rom(g, "extract");
		connect_parent_rom(g, "verify");
		connect_parent_rom(g, "fetchurl");
		connect_parent_rom(g, "fs_tool");
		connect_parent_rom(g, "chroot");
		connect_parent_rom(g, "curl.lib.so");
		connect_parent_rom(g, "init");
		connect_parent_rom(g, "depot_query");
		connect_parent_rom(g, "depot_download_manager");
		connect_parent_rom(g, "report_rom");
		connect_parent_rom(g, "vfs");
		connect_parent_rom(g, "lxip.lib.so");
		connect_parent_rom(g, "vfs_lxip.lib.so");
		connect_parent_rom(g, "vfs_pipe.lib.so");
		connect_parent_rom(g, "posix.lib.so");
		connect_parent_rom(g, "libssh.lib.so");
		connect_parent_rom(g, "libssl.lib.so");
		connect_parent_rom(g, "libcrypto.lib.so");
		connect_parent_rom(g, "zlib.lib.so");
		connect_parent_rom(g, "libarchive.lib.so");
		connect_parent_rom(g, "liblzma.lib.so");
		connect_parent_rom(g, "config",       "depot_download.config");
		connect_parent_rom(g, "installation", "config -> install");
		connect_report(g);

		auto gen_relabeled_log = [&] (Label const &label, Label const &relabeled) {
			gen_named_node(g, "log", label, [&] {
				g.node("parent", [&] { g.attribute("label", relabeled); }); }); };

		/* shorten LOG-session labels to reduce the debug-output noise */
		gen_relabeled_log("dynamic -> fetchurl", "fetchurl");
		gen_relabeled_log("dynamic -> verify",   "verify");
		gen_relabeled_log("dynamic -> extract",  "extract");

		g.node("nic", [&] {
			gen_named_node(g, "child", "nic_router"); });
	});
}
