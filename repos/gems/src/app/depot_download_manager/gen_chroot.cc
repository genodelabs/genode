/*
 * \brief  XML configuration for the chroot component
 * \author Norman Feske
 * \date   2017-12-08
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include "xml.h"

void Depot_download_manager::gen_chroot_start_content(Xml_generator &xml,
                                                      Archive::User const &user)
{
	gen_common_start_content(xml, Path("/depot/", user),
	                         Cap_quota{100}, Ram_quota{1*1024*1024 + 32*1024});

	xml.node("binary", [&] () { xml.attribute("name", "chroot"); });

	xml.node("config", [&] () {
		xml.node("default-policy", [&] () {
			xml.attribute("path", Path("/", user));
			xml.attribute("writeable", "yes");
		});
	});

	xml.node("provides", [&] () {
		xml.node("service", [&] () {
			xml.attribute("name", File_system::Session::service_name()); }); });

	xml.node("route", [&] () {
		xml.node("service", [&] () {
			xml.attribute("name", File_system::Session::service_name());
			xml.node("parent", [&] () {
				xml.attribute("label", "depot_rw"); });
		});
		gen_parent_unscoped_rom_route(xml, "chroot");
		gen_parent_unscoped_rom_route(xml, "ld.lib.so");
		gen_parent_route<Cpu_session>(xml);
		gen_parent_route<Pd_session> (xml);
		gen_parent_route<Log_session>(xml);
	});
}
