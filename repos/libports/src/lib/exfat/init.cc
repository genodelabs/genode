/*
 * \brief   libc_fuse_exfat
 * \author  Josef Soentgen
 * \date    2013-11-11
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/log.h>

#include <fuse_private.h>

extern "C" {

#include <exfat.h>

extern struct fuse_operations fuse_exfat_ops;

struct fuse_chan *fc;
struct fuse      *fh;

struct exfat ef;

}


bool Fuse::init_fs(void)
{
	Genode::log("libc_fuse_exfat: try to mount /dev/blkdev...");

	int err = exfat_mount(&ef, "/dev/blkdev", "");
	if (err) {
		Genode::error("libc_fuse_exfat: could not mount /dev/blkdev");
		return false;
	}

	fh = fuse_new(fc, NULL, &fuse_exfat_ops, sizeof (struct fuse_operations), NULL);
	if (fh == 0) {
		Genode::error("libc_fuse_exfat: fuse_new() failed");
		return false;
	}

	return true;
}


void Fuse::deinit_fs(void)
{
	Genode::log("libc_fuse_exfat: umount /dev/blkdev...");
	exfat_unmount(&ef);
}


void Fuse::sync_fs(void) { }


bool Fuse::support_symlinks(void)
{
	return false;
}
