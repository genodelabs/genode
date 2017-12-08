/*
 * \brief  Utilities for generating XML
 * \author Norman Feske
 * \date   2018-01-11
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _GENERATE_XML_H_
#define _GENERATE_XML_H_

/* Genode includes */
#include <util/xml_generator.h>
#include <rom_session/rom_session.h>
#include <cpu_session/cpu_session.h>
#include <log_session/log_session.h>
#include <pd_session/pd_session.h>
#include <timer_session/timer_session.h>
#include <nic_session/nic_session.h>
#include <report_session/report_session.h>
#include <file_system_session/file_system_session.h>

/* local includes */
#include "import.h"

namespace Depot_download_manager {

	template <typename SESSION>
	static inline void gen_parent_service(Xml_generator &xml)
	{
		xml.node("service", [&] () {
			xml.attribute("name", SESSION::service_name()); });
	};

	template <typename SESSION>
	static inline void gen_parent_route(Xml_generator &xml)
	{
		xml.node("service", [&] () {
			xml.attribute("name", SESSION::service_name());
			xml.node("parent", [&] () { });
		});
	}

	static inline void gen_parent_unscoped_rom_route(Xml_generator  &xml,
	                                                 Rom_name const &name)
	{
		xml.node("service", [&] () {
			xml.attribute("name", Rom_session::service_name());
			xml.attribute("unscoped_label", name);
			xml.node("parent", [&] () {
				xml.attribute("label", name); });
		});
	}

	static inline void gen_parent_rom_route(Xml_generator  &xml,
	                                        Rom_name const &name)
	{
		xml.node("service", [&] () {
			xml.attribute("name", Rom_session::service_name());
			xml.attribute("label", name);
			xml.node("parent", [&] () {
				xml.attribute("label", name); });
		});
	}

	static inline void gen_common_start_content(Xml_generator   &xml,
	                                            Rom_name  const &name,
	                                            Cap_quota const  caps,
	                                            Ram_quota const  ram)
	{
		xml.attribute("name", name);
		xml.attribute("caps", caps.value);
		xml.node("resource", [&] () {
			xml.attribute("name", "RAM");
			xml.attribute("quantum", String<64>(Number_of_bytes(ram.value)));
		});
	}

	void gen_depot_query_start_content(Xml_generator &,
	                                   Xml_node installation,
	                                   Archive::User const &,
	                                   Depot_query_version);

	void gen_fetchurl_start_content(Xml_generator &, Import const &, Url const &,
	                                Fetchurl_version);

	void gen_verify_start_content(Xml_generator &, Import const &, Path const &);

	void gen_chroot_start_content(Xml_generator &, Archive::User const &);

	void gen_extract_start_content(Xml_generator &, Import const &,
	                               Path const &, Archive::User const &);
}

#endif /* _GENERATE_XML_H_ */
