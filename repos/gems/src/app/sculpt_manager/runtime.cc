/*
 * \brief  Sculpt runtime-configuration utilites
 * \author Norman Feske
 * \date   2018-07-06
 *
 * This file is used to compile all '*.cc' files in the 'runtime/' directory at
 * once, which saves about 50% compile time.
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <runtime/chroot.cc>
#include <runtime/depot_query.cc>
#include <runtime/launcher_query.cc>
#include <runtime/e2fs.cc>
#include <runtime/inspect_view.cc>
#include <runtime/file_system.cc>
#include <runtime/fs_rom.cc>
#include <runtime/gpt_write.cc>
#include <runtime/nic_drv.cc>
#include <runtime/nic_router.cc>
#include <runtime/prepare.cc>
#include <runtime/ram_fs.cc>
#include <runtime/runtime_view.cc>
#include <runtime/update.cc>
#include <runtime/wifi_drv.cc>
#include <runtime/fs_tool.cc>
