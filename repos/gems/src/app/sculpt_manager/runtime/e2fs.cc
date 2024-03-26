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

	void gen_e2fs_start_content(Xml_generator &, Storage_target const &,
	                            Rom_name const &, auto const &);

	void gen_arg(Xml_generator &xml, auto const &arg)
	{
		xml.node("arg", [&] { xml.attribute("value", arg); });
	}
}


void Sculpt::gen_e2fs_start_content(Xml_generator        &xml,
                                    Storage_target const &target,
                                    Rom_name       const &tool,
                                    auto           const &gen_args_fn)
{
	gen_common_start_content(xml, String<64>(target.label(), ".", tool),
	                         Cap_quota{500}, Ram_quota{100*1024*1024},
	                         Priority::STORAGE);

	gen_named_node(xml, "binary", tool);

	xml.node("config", [&] {
		xml.node("libc", [&] {
			xml.attribute("stdout", "/dev/log");
			xml.attribute("stderr", "/dev/log");
			xml.attribute("stdin",  "/dev/null");
			xml.attribute("rtc",    "/dev/rtc");
		});
		xml.node("vfs", [&] {
			gen_named_node(xml, "dir", "dev", [&] {
				gen_named_node(xml, "block", "block", [&] {
					xml.attribute("label", "default");
					xml.attribute("block_buffer_count", 128);
				});
				gen_named_node(xml, "inline", "rtc", [&] {
					xml.append("2018-01-01 00:01");
				});
				xml.node("null", [&] {});
				xml.node("log",  [&] {});
			});
		});
		gen_args_fn(xml);
	});

	xml.node("route", [&] {
		target.gen_block_session_route(xml);
		gen_parent_route<Cpu_session>    (xml);
		gen_parent_route<Pd_session>     (xml);
		gen_parent_route<Log_session>    (xml);
		gen_parent_route<Rom_session>    (xml);
		gen_parent_route<Timer::Session> (xml);
	});
}


void Sculpt::gen_fsck_ext2_start_content(Xml_generator &xml,
                                         Storage_target const &target)
{
	auto gen_args = [&] (Xml_generator &xml) {
		gen_arg(xml, "fsck.ext2");
		gen_arg(xml, "-yv");
		gen_arg(xml, "/dev/block");
	};

	gen_e2fs_start_content(xml, target, "e2fsck", gen_args);
}


void Sculpt::gen_mkfs_ext2_start_content(Xml_generator &xml,
                                         Storage_target const &target)
{
	auto gen_args = [&] (Xml_generator &xml) {
		gen_arg(xml, "mkfs.ext2");
		gen_arg(xml, "-F");
		gen_arg(xml, "/dev/block");
	};

	gen_e2fs_start_content(xml, target, "mke2fs", gen_args);
}


void Sculpt::gen_resize2fs_start_content(Xml_generator &xml,
                                         Storage_target const &target)
{
	auto gen_args = [&] (Xml_generator &xml) {
		gen_arg(xml, "resize2fs");
		gen_arg(xml, "-f");
		gen_arg(xml, "-p");
		gen_arg(xml, "/dev/block");
	};

	gen_e2fs_start_content(xml, target, "resize2fs", gen_args);
}
