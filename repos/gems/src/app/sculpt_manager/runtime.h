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
#include <model/storage_devices.h>
#include <model/storage_target.h>
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
	};

	void gen_chroot_child_content(Generator &, Child_name const &,
	                              Path const &, Writeable);

	struct Inspect_view_version { unsigned value; };
	void gen_inspect_view(Generator &, Storage_devices const &,
	                      File_system const &ram_fs_state, Inspect_view_version);

	void gen_fs_child_content(Generator &, Storage_target const &,
	                          File_system::Type);

	void gen_gpt_relabel_child_content(Generator &, Storage_device const &);
	void gen_gpt_expand_child_content (Generator &, Storage_device const &);

	struct Prepare_version { unsigned value; };
	void gen_prepare_child_content(Generator &, Prepare_version);

	struct Fs_tool_version { unsigned value; };
	struct File_operation_queue;
	void gen_fs_tool_child_content(Generator &, Fs_tool_version,
                                   File_operation_queue const &);

	void gen_update_child_content(Generator &);

	void gen_fsck_ext2_child_content(Generator &, Storage_target const &);
	void gen_mkfs_ext2_child_content(Generator &, Storage_target const &);
	void gen_resize2fs_child_content(Generator &, Storage_target const &);
}

#endif /* _RUNTIME_H_ */
