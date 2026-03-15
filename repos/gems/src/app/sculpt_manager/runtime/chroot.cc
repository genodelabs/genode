/*
 * \brief  Configuration for the chroot component
 * \author Norman Feske
 * \date   2018-05-18
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <runtime.h>

void Sculpt::gen_chroot_child_content(Generator &g, Child_name const &name,
                                      Path const &path, Writeable writeable)
{
	gen_child_attr(g, name, "chroot", Cap_quota{100}, Ram_quota{2*1024*1024},
	               Priority::STORAGE);

	g.node("config", [&] {
		g.node("default-policy", [&] {
			g.attribute("path", path);
			if (writeable == WRITEABLE)
				g.attribute("writeable", "yes");
		});
	});

	g.node("provides", [&] { g.node("fs"); });

	g.node("connect", [&] { connect_fs(g, "default_fs_rw"); });
}
