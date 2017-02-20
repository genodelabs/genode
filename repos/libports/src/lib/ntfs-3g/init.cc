/*
 * \brief   libc_fuse_ntfs-3g
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

#include <device.h>
#include <security.h>
#include <ntfs-3g_common.h>

extern struct fuse_operations ntfs_3g_ops;

struct fuse_chan *fc;
struct fuse      *fh;

extern ntfs_fuse_context_t **ntfs_fuse_ctx();
extern int ntfs_open(const char*);
extern void ntfs_close(void);

}


bool Fuse::init_fs(void)
{
	ntfs_set_locale();
	ntfs_log_set_handler(ntfs_log_handler_stderr);

	ntfs_fuse_context_t **ctx = ntfs_fuse_ctx();

	*ctx = reinterpret_cast<ntfs_fuse_context_t *>(malloc(sizeof (ntfs_fuse_context_t)));
	if (!*ctx) {
		Genode::error("out of memory");
		return false;
	}

	Genode::memset(*ctx, 0, sizeof (ntfs_fuse_context_t));
	(*ctx)->streams = NF_STREAMS_INTERFACE_NONE;
	(*ctx)->atime   = ATIME_RELATIVE;
	(*ctx)->silent  = TRUE;
	(*ctx)->recover = TRUE;

	/*
	*ctx = (ntfs_fuse_context_t) {
		.uid = 0,
		.gid = 0,
		.streams = NF_STREAMS_INTERFACE_NONE,
		.atime   = ATIME_RELATIVE,
		.silent  = TRUE,
		.recover = TRUE
	};
	*/

	Genode::log("libc_fuse_ntfs-3g: try to mount /dev/blkdev...");

	int err = ntfs_open("/dev/blkdev");
	if (err) {
		Genode::error("libc_fuse_ntfs-3g: could not mount /dev/blkdev");
		return false;
	}

	fh = fuse_new(fc, NULL, &ntfs_3g_ops, sizeof (ntfs_3g_ops), NULL);
	if (fh == 0) {
		Genode::error("libc_fuse_exfat: fuse_new() failed");
		ntfs_close();
		return false;
	}

	(*ctx)->mounted = TRUE;

	return true;
}


void Fuse::deinit_fs(void)
{
	Genode::log("libc_fuse_ntfs-3g: unmount /dev/blkdev...");
	ntfs_close();

	free(*ntfs_fuse_ctx());
}


void Fuse::sync_fs(void)
{
	Genode::log("libc_fuse_ntfs-3g: sync file system...");
	ntfs_device_sync((*ntfs_fuse_ctx())->vol->dev);
}


bool Fuse::support_symlinks(void)
{
	return true;
}
