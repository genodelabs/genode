/*
 * \brief  Utilities for generating runtime configurations
 * \author Norman Feske
 * \date   2018-05-18
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _RUNTIME_H_
#define _RUNTIME_H_

#include <xml.h>
#include <model/ram_fs_state.h>
#include <model/storage_devices.h>
#include <model/storage_target.h>
#include <model/nic_target.h>
#include <model/child_state.h>

namespace Sculpt {

	struct Runtime_config_generator : Interface
	{
		virtual void generate_runtime_config() = 0;
	};

	struct Runtime_info : Interface
	{
		using Version = Child_state::Version;

		/**
		 * Return true if specified child is present in the runtime subsystem
		 */
		virtual bool present_in_runtime(Start_name const &) const = 0;

		virtual bool abandoned_by_user(Start_name const &) const = 0;

		virtual Version restarted_version(Start_name const &) const = 0;

		virtual void gen_launched_deploy_start_nodes(Xml_generator &) const = 0;
	};

	void gen_chroot_start_content(Xml_generator &, Start_name const &,
	                              Path const &, Writeable);

	void gen_depot_query_start_content(Xml_generator &);
	void gen_launcher_query_start_content(Xml_generator &);

	void gen_runtime_view_start_content(Xml_generator &, Child_state const &,
	                                    double font_size);

	struct Inspect_view_version { unsigned value; };
	void gen_inspect_view(Xml_generator &, Storage_devices const &,
	                      Ram_fs_state const &, Inspect_view_version);

	void gen_runtime_view(Xml_generator &);

	void gen_fs_start_content(Xml_generator &, Storage_target const &,
	                          File_system::Type);

	void gen_fs_rom_start_content(Xml_generator &,
	                              Start_name const &, Start_name const &,
	                              Child_state const &);

	void gen_gpt_relabel_start_content(Xml_generator &, Storage_device const &);
	void gen_gpt_expand_start_content (Xml_generator &, Storage_device const &);

	void gen_nic_drv_start_content(Xml_generator &);
	void gen_wifi_drv_start_content(Xml_generator &);

	void gen_nic_router_start_content(Xml_generator &);
	void gen_nic_router_uplink(Xml_generator &, char const *);

	struct Prepare_version { unsigned value; };
	void gen_prepare_start_content(Xml_generator &, Prepare_version);

	struct Fs_tool_version { unsigned value; };
	struct File_operation_queue;
	void gen_fs_tool_start_content(Xml_generator &, Fs_tool_version,
                                   File_operation_queue const &);

	void gen_ram_fs_start_content(Xml_generator &, Ram_fs_state const &);

	void gen_update_start_content(Xml_generator &);

	void gen_fsck_ext2_start_content(Xml_generator &, Storage_target const &);
	void gen_mkfs_ext2_start_content(Xml_generator &, Storage_target const &);
	void gen_resize2fs_start_content(Xml_generator &, Storage_target const &);
}

#endif /* _RUNTIME_H_ */
