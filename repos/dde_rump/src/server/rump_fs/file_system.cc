/*
 * \brief  Rump initialization
 * \author Sebastian Sumpf
 * \date   2014-01-17
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include "file_system.h"

#include <rump/bootstrap.h>
#include <rump_fs/fs.h>
#include <util/string.h>
#include <util/hard_context.h>
#include <base/log.h>
#include <base/attached_rom_dataspace.h>

/**
 * We define our own fs arg structure to fit all sizes, we assume that 'fspec'
 * is the only valid argument and all other fields are unused.
 */
struct fs_args
{
	char *fspec;
	char  pad[150];

	fs_args() { Genode::memset(pad, 0, sizeof(pad)); }
};


namespace File_system { class Sync; };


static char const *fs_types[] = { RUMP_MOUNT_CD9660, RUMP_MOUNT_EXT2FS,
                                  RUMP_MOUNT_FFS, RUMP_MOUNT_MSDOS,
                                  RUMP_MOUNT_NTFS, RUMP_MOUNT_UDF, 0 };

typedef Genode::String<16> Fs_type;


static bool _supports_symlinks;


static bool _check_type(Fs_type const &type)
{
	for (int i = 0; fs_types[i]; i++)
		if (!Genode::strcmp(type.string(), fs_types[i]))
			return true;
	return false;
}


static void _print_types()
{
	Genode::error("fs types:");
	for (int i = 0; fs_types[i]; ++i)
		Genode::error("\t", fs_types[i]);
}


static bool check_symlinks(Fs_type const &fs_type)
{
	return (fs_type == RUMP_MOUNT_EXT2FS)
	    || (fs_type == RUMP_MOUNT_FFS);
}


static bool check_read_only(Fs_type const &fs_type)
{
	return fs_type == RUMP_MOUNT_CD9660;
}


void File_system::init(Genode::Env &env, Genode::Allocator &alloc, Genode::Xml_node config)
{
	Fs_type const fs_type = config.attribute_value("fs", Fs_type());

	if (!_check_type(fs_type)) {
		Genode::error("Invalid or no file system given (use \'<config fs=\"<fs type>\"/>)");
		_print_types();
		throw Genode::Exception();
	}
	Genode::log("Using ", fs_type, " as file system");

	/* make Genode env and heap known to the rump kernel */
	rump_bootstrap_init(env, alloc);

	/* start rump kernel */
	rump_init();

	/* register block device */ 
	rump_pub_etfs_register(GENODE_DEVICE, GENODE_BLOCK_SESSION, RUMP_ETFS_BLK);

	/* mount into extra-terrestrial-file system */
	struct fs_args args;
	int            opts = check_read_only(fs_type) ? RUMP_MNT_RDONLY : 0;

	args.fspec =  (char *)GENODE_DEVICE;
	if (rump_sys_mount(fs_type.string(), "/", opts, &args, sizeof(args)) == -1) {
		Genode::error("Mounting '", fs_type, "' file system failed (errno ", errno, " )");
		throw Genode::Exception();
	}

	/* check support for symlinks */
	_supports_symlinks = check_symlinks(fs_type);
}


bool File_system::supports_symlinks() { return _supports_symlinks; }
