/*
 * \brief  Jitterentropy based random file system
 * \author Josef Soentgen
 * \date   2014-08-19
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <libc-plugin/vfs.h>

/* local includes */
#include <vfs_jitterentropy.h>


struct Jitterentropy_factory : Libc::File_system_factory
{
	Jitterentropy_factory() : File_system_factory("jitterentropy") { }

	Vfs::File_system *create(Genode::Xml_node node)
	{
		return new (Genode::env()->heap()) Jitterentropy_file_system(node);
	}
};


extern "C" Libc::File_system_factory *Libc_file_system_factory(void)
{
	static Jitterentropy_factory factory;
	return &factory;
}
