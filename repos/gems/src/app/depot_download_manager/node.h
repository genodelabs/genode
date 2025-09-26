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

#ifndef _NODE_H_
#define _NODE_H_

/* Genode includes */
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
#include "job.h"

namespace Depot_download_manager {

	template <typename SESSION>
	static inline void gen_parent_service(Generator &g)
	{
		g.node("service", [&] {
			g.attribute("name", SESSION::service_name()); });
	};

	template <typename SESSION>
	static inline void gen_parent_route(Generator &g)
	{
		g.node("service", [&] {
			g.attribute("name", SESSION::service_name());
			g.node("parent", [&] { });
		});
	}

	static inline void gen_parent_unscoped_rom_route(Generator      &g,
	                                                 Rom_name const &name)
	{
		g.node("service", [&] {
			g.attribute("name", Rom_session::service_name());
			g.attribute("unscoped_label", name);
			g.node("parent", [&] {
				g.attribute("label", name); });
		});
	}

	static inline void gen_parent_rom_route(Generator      &g,
	                                        Rom_name const &name)
	{
		g.node("service", [&] {
			g.attribute("name", Rom_session::service_name());
			g.attribute("label", name);
			g.node("parent", [&] {
				g.attribute("label", name); });
		});
	}

	static inline void gen_common_start_content(Generator       &g,
	                                            Rom_name  const &name,
	                                            Cap_quota const  caps,
	                                            Ram_quota const  ram)
	{
		g.attribute("name", name);
		g.attribute("caps", caps.value);
		g.node("resource", [&] {
			g.attribute("name", "RAM");
			g.attribute("quantum", String<64>(Number_of_bytes(ram.value)));
		});
	}

	/*
	 * Common start-node content shared by 'stage' and 'commit' steps
	 */
	static inline void gen_fs_tool_start_content(Generator           &g,
	                                             Path          const &user_path,
	                                             Archive::User const &user,
	                                             auto          const &config_fn)
	{
		g.node("binary", [&] { g.attribute("name", "fs_tool"); });

		g.node("config", [&] {
			g.attribute("verbose", "yes");
			g.attribute("exit",    "yes");

			g.node("vfs", [&] {
				g.node("dir", [&] { g.attribute("name", user);
					g.node("fs", [&] { g.attribute("label", "/"); }); }); });
			config_fn();
		});

		g.node("route", [&] {
			g.node("service", [&] {
				g.attribute("name", File_system::Session::service_name());
				g.node("child", [&] {
					g.attribute("name", user_path); });
			});
			gen_parent_unscoped_rom_route(g, "fs_tool");
			gen_parent_unscoped_rom_route(g, "ld.lib.so");
			gen_parent_rom_route(g, "vfs.lib.so");
			gen_parent_route<Cpu_session>(g);
			gen_parent_route<Pd_session> (g);
			gen_parent_route<Log_session>(g);
		});
	}

	void gen_depot_query_start_content(Generator &,
	                                   Node const &installation,
	                                   Archive::User const &,
	                                   Depot_query_version,
	                                   List_model<Job> const &);

	void gen_fetchurl_start_content(Generator &, Import const &, Url const &,
	                                Pubkey_known, Fetchurl_version);

	void gen_verify_start_content(Generator &, Import const &, Path const &);

	void gen_chroot_start_content(Generator &, Archive::User const &);

	void gen_extract_start_content(Generator &, Import const &,
	                               Path const &, Archive::User const &);

	void gen_stage_start_content(Generator &, Import const &,
	                             Path const &, Archive::User const &);

	void gen_commit_start_content(Generator &, Import const &,
	                              Path const &, Archive::User const &);
}

#endif /* _NODE_H_ */
