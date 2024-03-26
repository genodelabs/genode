/*
 * \brief  XML configuration for the depot-download subsystem
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

void Sculpt::gen_update_start_content(Xml_generator &xml)
{
	gen_common_start_content(xml, "update",
	                         Cap_quota{2000}, Ram_quota{64*1024*1024},
	                         Priority::STORAGE);

	gen_named_node(xml, "binary", "init");

	xml.node("route", [&] {

		typedef String<32> Label;
		auto gen_fs = [&] (Label const &label, Label const &server) {
			gen_service_node<::File_system::Session>(xml, [&] {
				xml.attribute("label", label);
				gen_named_node(xml, "child", server); }); };

		/* connect file-system sessions to chroot instances */
		gen_fs("depot",  "depot_rw");
		gen_fs("public", "public_rw");

		gen_parent_rom_route(xml, "ld.lib.so");
		gen_parent_rom_route(xml, "vfs.lib.so");
		gen_parent_rom_route(xml, "libc.lib.so");
		gen_parent_rom_route(xml, "libm.lib.so");
		gen_parent_rom_route(xml, "extract");
		gen_parent_rom_route(xml, "verify");
		gen_parent_rom_route(xml, "fetchurl");
		gen_parent_rom_route(xml, "chroot");
		gen_parent_rom_route(xml, "curl.lib.so");
		gen_parent_rom_route(xml, "init");
		gen_parent_rom_route(xml, "depot_query");
		gen_parent_rom_route(xml, "depot_download_manager");
		gen_parent_rom_route(xml, "report_rom");
		gen_parent_rom_route(xml, "vfs");
		gen_parent_rom_route(xml, "lxip.lib.so");
		gen_parent_rom_route(xml, "vfs_lxip.lib.so");
		gen_parent_rom_route(xml, "posix.lib.so");
		gen_parent_rom_route(xml, "libssh.lib.so");
		gen_parent_rom_route(xml, "libssl.lib.so");
		gen_parent_rom_route(xml, "libcrypto.lib.so");
		gen_parent_rom_route(xml, "zlib.lib.so");
		gen_parent_rom_route(xml, "libarchive.lib.so");
		gen_parent_rom_route(xml, "liblzma.lib.so");
		gen_parent_rom_route(xml, "config",       "depot_download.config");
		gen_parent_rom_route(xml, "installation", "config -> managed/installation");
		gen_parent_route<Cpu_session>    (xml);
		gen_parent_route<Pd_session>     (xml);
		gen_parent_route<Rm_session>     (xml);
		gen_parent_route<Timer::Session> (xml);
		gen_parent_route<Report::Session>(xml);

		auto gen_relabeled_log = [&] (Label const &label, Label const &relabeled) {
			gen_service_node<Log_session>(xml, [&] {
				xml.attribute("label", label);
				xml.node("parent", [&] {
					xml.attribute("label", relabeled); }); }); };

		/* shorten LOG-session labels to reduce the debug-output noise */
		gen_relabeled_log("dynamic -> fetchurl", "fetchurl");
		gen_relabeled_log("dynamic -> verify",   "verify");
		gen_relabeled_log("dynamic -> extract",  "extract");
		gen_parent_route<Log_session>(xml);

		gen_service_node<Nic::Session>(xml, [&] {
			gen_named_node(xml, "child", "nic_router"); });
	});
}
