/*
 * \brief  Minimal VFS plugin for bringing up WLAN driver
 * \author Josef Soentgen
 * \date   2022-02-20
 *
 * The sole purpose of this VFS plugin is to call 'Lx_kit::initialize_env'
 * at the right time before 'env.exec_static_constructors' is executed.
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <os/vfs.h>
#include <vfs/single_file_system.h>

/* DDE Linux includes */
#include <lx_kit/init.h>


namespace Vfs_wlan
{
	using namespace Vfs;
	using namespace Genode;

	struct File_system;
}

struct Vfs_wlan::File_system : Single_file_system
{

	File_system(Vfs::Env &env, Xml_node config)
	:
		Single_file_system { Vfs::Node_type::CONTINUOUS_FILE, name(),
		                     Vfs::Node_rwx::ro(), config }
	{
		/*
		 * Various ports of a DDE Linux based library rely on the
		 * 'env' being set before any static constructor is executed.
		 * So we set it here and wait for the user of the library to
		 * execute the constructors at the proper time.
		 */

		Lx_kit::initialize(env.env());
	}

	Open_result open(char const  *, unsigned,
	                 Vfs::Vfs_handle **,
	                 Allocator   &) override
	{
		return OPEN_ERR_UNACCESSIBLE;
	}

	Stat_result stat(char const *, Stat &) override
	{
		return STAT_ERR_NO_ENTRY;
	}

	static char const *name() { return "wlan"; }
	char        const *type() override { return name(); }
};


/**************************
 ** VFS plugin interface **
 **************************/

extern "C" Vfs::File_system_factory *vfs_file_system_factory(void)
{
	struct Factory : Vfs::File_system_factory
	{
		Vfs::File_system *create(Vfs::Env &vfs_env,
		                         Genode::Xml_node node) override
		{
			static Vfs::File_system *fs =
				new (vfs_env.alloc()) Vfs_wlan::File_system(vfs_env, node);
			return fs;
		}
	};

	static Factory factory;
	return &factory;
}
