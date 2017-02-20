/*
 * \brief   libc_fuse_ext2
 * \author  Josef Soentgen
 * \date    2013-11-11
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>
#include <util/string.h>

#include <fuse.h>
#include <fuse_private.h>

/* libc includes */
#include <stdlib.h>

extern "C" {

#include <fuse-ext2.h>
#include <ext2fs/ext2fs.h>

extern struct fuse_operations ext2fs_ops;

struct fuse_chan *fc;
struct fuse      *fh;

static ext2_filsys e2fs;
static struct extfs_data extfs_data;

char mnt_point[] = "/";
char options[]   = "";
char device[]    = "/dev/blkdev";
char volname[]   = "ext2_volume";

}


bool Fuse::init_fs(void)
{
	Genode::log("libc_fuse_ext2: try to mount /dev/blkdev...");

	int err = ext2fs_open("/dev/blkdev",  EXT2_FLAG_RW, 0, 0, unix_io_manager, &e2fs);
	if (err) {
		Genode::error("libc_fuse_ext2: could not mount /dev/blkdev, error: ", err);
		return false;
	}

	errcode_t rc = ext2fs_read_bitmaps(e2fs);
	if (rc) {
		Genode::error("libc_fuse_ext2: error while reading bitmaps");
		ext2fs_close(e2fs);
		return false;
	}

	/* set 1 to enable debug messages */
	extfs_data.debug      = 0;

	extfs_data.silent     = 0;
	extfs_data.force      = 0;
	extfs_data.readonly   = 0;
	extfs_data.last_flush = 0;
	extfs_data.mnt_point  = mnt_point;
	extfs_data.options    = options;
	extfs_data.device     = device;
	extfs_data.volname    = volname;
	extfs_data.e2fs       = e2fs;

	fh = fuse_new(fc, NULL, &ext2fs_ops, sizeof (ext2fs_ops), &extfs_data);
	if (fh == 0) {
		Genode::error("libc_fuse_ext2: fuse_new() failed");
		return false;
	}

	return true;
}


void Fuse::deinit_fs(void)
{
	Genode::log("libc_fuse_ext2: unmount /dev/blkdev...");
	ext2fs_close(e2fs);

	free(fh);
}


void Fuse::sync_fs(void)
{
	Genode::log("libc_fuse_ext2: sync file system...");
	ext2fs_flush(e2fs);
}


bool Fuse::support_symlinks(void)
{
	return true;
}
