/*
 * \brief  XML configuration for invoking e2fstools
 * \author Norman Feske
 * \date   2018-05-02
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <runtime.h>

namespace Sculpt {

	template <typename GEN_ARGS_FN>
	void gen_e2fs_start_content(Xml_generator &, Storage_target const &,
	                            Rom_name const &, GEN_ARGS_FN const &);

}


template <typename GEN_ARGS_FN>
void Sculpt::gen_e2fs_start_content(Xml_generator        &xml,
                                    Storage_target const &target,
                                    Rom_name       const &tool,
                                    GEN_ARGS_FN    const &gen_args_fn)
{
	gen_common_start_content(xml, String<64>(target.label(), ".", tool),
	                         Cap_quota{500}, Ram_quota{100*1024*1024});

	gen_named_node(xml, "binary", "noux");

	xml.node("config", [&] () {
		xml.attribute("stdout", "/dev/log");
		xml.attribute("stderr", "/dev/log");
		xml.attribute("stdin",  "/dev/null");
		xml.node("fstab", [&] () {
			gen_named_node(xml, "tar", "e2fsprogs-minimal.tar");
			gen_named_node(xml, "dir", "dev", [&] () {
				gen_named_node(xml, "block", "block", [&] () {
					xml.attribute("label", "default");
					xml.attribute("block_buffer_count", 128);
				});
				xml.node("null", [&] () {});
				xml.node("log",  [&] () {});
			});
		});
		gen_named_node(xml, "start", Rom_name("/bin/", tool), [&] () {
			gen_args_fn(xml); });
	});

	xml.node("route", [&] () {
		gen_parent_rom_route(xml, "noux");
		gen_parent_rom_route(xml, "ld.lib.so");
		gen_parent_route<Cpu_session>    (xml);
		gen_parent_route<Pd_session>     (xml);
		gen_parent_route<Log_session>    (xml);
		gen_parent_route<Rom_session>    (xml);
		gen_parent_route<Timer::Session> (xml);

		target.gen_block_session_route(xml);
	});
}


void Sculpt::gen_fsck_ext2_start_content(Xml_generator &xml,
                                         Storage_target const &target)
{
	auto gen_args = [&] (Xml_generator &xml) {
		xml.node("arg", [&] () { xml.attribute("value", "-yv"); });
		xml.node("arg", [&] () { xml.attribute("value", "/dev/block"); });
	};

	gen_e2fs_start_content(xml, target, "fsck.ext2", gen_args);
}


void Sculpt::gen_mkfs_ext2_start_content(Xml_generator &xml,
                                         Storage_target const &target)
{
	auto gen_args = [&] (Xml_generator &xml) {
		xml.node("arg", [&] () {
			xml.attribute("value", "/dev/block"); }); };

	gen_e2fs_start_content(xml, target, "mkfs.ext2", gen_args);
}


void Sculpt::gen_resize2fs_start_content(Xml_generator &xml,
                                         Storage_target const &target)
{
	auto gen_args = [&] (Xml_generator &xml) {

		auto gen_arg = [&] (char const *arg) {
			xml.node("arg", [&] () {
				xml.attribute("value", arg); }); };

		gen_arg("-f");
		gen_arg("-p");
		gen_arg("/dev/block");
	};

	gen_e2fs_start_content(xml, target, "resize2fs", gen_args);
}
