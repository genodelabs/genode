/*
 * \brief  XML configuration for the chroot component
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

void Sculpt::gen_chroot_start_content(Xml_generator &xml, Start_name const &name,
                                      Path const &path, Writeable writeable)
{
	gen_common_start_content(xml, name,
	                         Cap_quota{100}, Ram_quota{2*1024*1024},
	                         Priority::STORAGE);

	gen_named_node(xml, "binary", "chroot");

	xml.node("config", [&] {
		xml.node("default-policy", [&] {
			xml.attribute("path", path);
			if (writeable == WRITEABLE)
				xml.attribute("writeable", "yes");
		});
	});

	gen_provides<::File_system::Session>(xml);

	xml.node("route", [&] {

	 	gen_service_node<::File_system::Session>(xml, [&] {
			gen_named_node(xml, "child", "default_fs_rw"); });

		gen_parent_rom_route(xml, "chroot");
		gen_parent_rom_route(xml, "ld.lib.so");
		gen_parent_route<Cpu_session>(xml);
		gen_parent_route<Pd_session> (xml);
		gen_parent_route<Log_session>(xml);
	});
}
