/*
 * \brief  Rump initialization
 * \author Sebastian Sumpf
 * \date   2014-01-17
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include "file_system.h"

#include <rump/env.h>
#include <rump_fs/fs.h>
#include <util/string.h>
#include <util/hard_context.h>
#include <base/log.h>
#include <base/attached_rom_dataspace.h>

/**
 * We define our own fs arg structure to fit all sizes used by the different
 * file system implementations, we assume that 'fspec' * is the only valid
 * argument and all other fields are unused.
 */
struct fs_args
{
	char *fspec;
	char  pad[164];

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


void File_system::init()
{
	Fs_type const fs_type = Rump::env().config_rom().xml().attribute_value("fs", Fs_type());

	if (!_check_type(fs_type)) {
		Genode::error("Invalid or no file system given (use \'<config fs=\"<fs type>\"/>)");
		_print_types();
		throw Genode::Exception();
	}

	Genode::log("Using ", fs_type, " as file system");

	size_t const avail = Rump::env().env().ram().avail_ram().value;
	rump_set_memlimit(avail);

	/* start rump kernel */
	try         { rump_init(); }
	catch (...) { throw Genode::Exception(); }

	/* register block device */ 
	rump_pub_etfs_register(GENODE_DEVICE, GENODE_BLOCK_SESSION, RUMP_ETFS_BLK);

	/* create mount directory */
	rump_sys_mkdir(GENODE_MOUNT_DIR, 0777);

	/* check support for symlinks */
	_supports_symlinks = check_symlinks(fs_type);

	/*
	 * Try to mount the file system just to check if it
	 * is working as intended. In case it is not that gives
	 * us a change to react upon before any client may
	 * hang.
	 */
	try {
		mount_fs();
		unmount_fs();
	} catch (...) {
		Genode::error("dry mount attempt failed, aborting");
		throw Genode::Exception();
	}
}


static int root_fd = -42;


void File_system::mount_fs()
{
	/* mount into extra-terrestrial-file system */
	struct fs_args args;
	args.fspec = (char *)GENODE_DEVICE;

	Fs_type const fs_type = Rump::env().config_rom().xml().attribute_value("fs", Fs_type());

	int opts  = check_read_only(fs_type) ? RUMP_MNT_RDONLY : 0;
	    opts |= RUMP_MNT_NOATIME;

	if (root_fd == -42) {
		root_fd = rump_sys_open("/", O_DIRECTORY | O_RDONLY);
		if (root_fd == -1) {
			Genode::error("opening root directory failed");
			throw Genode::Exception();
		}
	}

	int err = rump_sys_mount(fs_type.string(), GENODE_MOUNT_DIR,
	                         opts, &args, sizeof(args));
	if (err == -1) {
		Genode::error("mounting file system failed (errno ", errno, " )");
		throw Genode::Exception();
	}

	int const mnt_fd = rump_sys_open(GENODE_MOUNT_DIR,  O_DIRECTORY | O_RDONLY);
	if (mnt_fd == -1) {
		Genode::error("opening mnt directory failed");
		throw Genode::Exception();
	}

	err = rump_sys_fchroot(mnt_fd);
	if (err == -1) {
		Genode::error("fchroot to '", GENODE_MOUNT_DIR, "' failed ",
		              "(errno ", errno, " )");
		throw Genode::Exception();
	}
}


void File_system::unmount_fs()
{
	/* try to flush all outstanding modifications */
	rump_sys_sync();

	int err = rump_sys_fchroot(root_fd);
	if (err == -1) {
		Genode::error("fchroot to '/' failed ", "(errno ", errno, " )");
		throw Genode::Exception();
	}

	bool const force = true;

	err = rump_sys_unmount(GENODE_MOUNT_DIR, force ? RUMP_MNT_FORCE : 0);
	if (err == -1) {
		Genode::error("unmounting file system failed (errno ", errno, " )");
		throw Genode::Exception();
	}
}


bool File_system::supports_symlinks() { return _supports_symlinks; }
