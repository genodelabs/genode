/*
 * \brief  Libc config
 * \author Norman Feske
 * \date   2025-03-26
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LIBC__INTERNAL__CONFIG_H_
#define _LIBC__INTERNAL__CONFIG_H_

/* Genode includes */
#include <util/xml_node.h>
#include <base/component.h>

/* libc includes */
#include <unistd.h>
#include <internal/types.h>

namespace Libc { class Config; }


struct Libc::Config
{
	using Path = String<Vfs::MAX_PATH_LEN>;

	bool   update_mtime, cloned;
	pid_t  pid;
	Path   rtc, rng, pipe, socket, nameserver;
	size_t stack_size;

	static Config _from_libc_xml(Xml_node const &libc)
	{
		Path const socket = libc.attribute_value("socket", Path());
		Path const default_nameserver { socket, "/nameserver" };

		size_t stack_size = Component::stack_size();
		libc.with_optional_sub_node("stack", [&] (Xml_node stack) {
			stack_size = stack.attribute_value("size", Number_of_bytes(0)); });

		return {
			.update_mtime = libc.attribute_value("update_mtime", true),
			.cloned       = libc.attribute_value("cloned", false),
			.pid          = libc.attribute_value("pid",    pid_t(0)),
			.rtc          = libc.attribute_value("rtc",    Path()),
			.rng          = libc.attribute_value("rng",    Path()),
			.pipe         = libc.attribute_value("pipe",   Path()),
			.socket       = socket,
			.nameserver   = libc.attribute_value("nameserver_file",
			                                     default_nameserver),
			.stack_size   = stack_size,
		};
	}

	static Config from_xml(Xml_node const &config)
	{
		Config result { };
		config.with_optional_sub_node("libc", [&] (Xml_node const &libc) {
			result = _from_libc_xml(libc); });
		return result;
	}
};


namespace Libc {

	static inline void with_vfs_config(Node const &config, auto const &fn)
	{
		config.with_sub_node("vfs",
			[&] (Node const &vfs_config) { fn(vfs_config); },
			[&] {
				config.with_sub_node("libc",
					[&] (Node const &libc) {
						libc.with_sub_node("vfs",
							[&] (Node const &vfs_config) {
								warning("'<config> <libc> <vfs/>' is deprecated, "
								        "please move to '<config> <vfs/>'");
								fn(vfs_config);
							},
							[&] { fn(Node()); });
					},
					[&] { fn(Node()); });
			});
	}
}

#endif /* _LIBC__INTERNAL__CONFIG_H_ */
