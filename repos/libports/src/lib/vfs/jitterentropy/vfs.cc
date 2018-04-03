/*
 * \brief  Jitterentropy based random file system
 * \author Josef Soentgen
 * \date   2014-08-19
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <vfs/file_system_factory.h>

/* local includes */
#include <vfs_jitterentropy.h>


struct Jitterentropy_factory : Vfs::File_system_factory
{
	Vfs::File_system *create(Vfs::Env &env, Genode::Xml_node node) override
	{
		return new (env.alloc()) Jitterentropy_file_system(env.alloc(), node);
	}
};


extern "C" Vfs::File_system_factory *vfs_file_system_factory(void)
{
	static Jitterentropy_factory factory;
	return &factory;
}
