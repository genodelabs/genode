/*
 * \brief libfuse private API
 * \author Josef Soentgen
 * \date 2013-11-07
 *     */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__FUSE_PRIVATE_H_
#define _INCLUDE__FUSE_PRIVATE_H_

#include "fuse.h"

#ifdef __cplusplus
extern "C" {
#endif

struct fuse_dirhandle;
struct fuse_args;

struct fuse_session {
	void *args;
};

struct fuse_chan {
	char *dir;
	struct fuse_args *args;
};

struct fuse_config {
	uid_t   uid;
	gid_t   gid;
	pid_t   pid;
	mode_t  umask;
	int     set_mode;
	int     set_uid;
	int     set_gid;
};

typedef struct fuse_dirhandle {
	fuse_fill_dir_t  filler;
	void            *buf;
	size_t           size;
	off_t            offset;
} *fuse_dirh_t;


struct fuse {
	struct fuse_chan	*fc;
	struct fuse_operations	op;
	struct fuse_args        *args;

	struct fuse_config	conf;
	struct fuse_session	se;

	fuse_fill_dir_t         filler;

	void *userdata;

	/* Block_session info */
	uint32_t block_size;
	uint64_t block_count;
};

#ifdef __cplusplus
}
#endif

namespace Fuse {

	struct fuse* fuse();

	bool initialized();

	/**
	 * Initialize the file system
	 *
	 * Mount the medium containg the file system, e.g. by
	 * using a Block_session connection, and call the file system
	 * init function as well as initialize needed fuse structures.
	 */
	bool init_fs();

	/**
	 * Deinitialize the file system
	 *
	 * Unmount the medium, call the file system cleanup function
	 * and free all fuse structures.
	 */
	void deinit_fs();

	/**
	 * Synchronize the file system
	 *
	 * Request the file system to flush all internal caches
	 * to disk.
	 */
	void sync_fs();

	/**
	 * FUSE File system implementation supports symlinks
	 */
	bool support_symlinks();

	/* list of FUSE operations as of version 2.6 */
	enum Fuse_operations {
		FUSE_OP_GETATTR     =  0,
		FUSE_OP_READLINK    =  1,
		FUSE_OP_GETDIR      =  2,
		FUSE_OP_MKNOD       =  3,
		FUSE_OP_MKDIR       =  4,
		FUSE_OP_UNLINK      =  5,
		FUSE_OP_RMDIR       =  6,
		FUSE_OP_SYMLINK     =  7,
		FUSE_OP_RENAME      =  8,
		FUSE_OP_LINK        =  9,
		FUSE_OP_CHMOD       = 10,
		FUSE_OP_CHOWN       = 11,
		FUSE_OP_TRUNCATE    = 12,
		FUSE_OP_UTIME       = 13,
		FUSE_OP_OPEN        = 14,
		FUSE_OP_READ        = 15,
		FUSE_OP_WRITE       = 16,
		FUSE_OP_STATFS      = 17,
		FUSE_OP_FLUSH       = 18,
		FUSE_OP_RELEASE     = 19,
		FUSE_OP_FSYNC       = 20,
		FUSE_OP_SETXATTR    = 21,
		FUSE_OP_GETXATTR    = 22,
		FUSE_OP_LISTXATTR   = 23,
		FUSE_OP_REMOVEXATTR = 24,
		FUSE_OP_OPENDIR     = 25,
		FUSE_OP_READDIR     = 26,
		FUSE_OP_RELEASEDIR  = 27,
		FUSE_OP_FSYNCDIR    = 28,
		FUSE_OP_INIT        = 29,
		FUSE_OP_DESTROY     = 30,
		FUSE_OP_ACCESS      = 31,
		FUSE_OP_CREATE      = 32,
		FUSE_OP_FTRUNCATE   = 33,
		FUSE_OP_FGETATTR    = 34,
		FUSE_OP_LOCK        = 35,
		FUSE_OP_UTIMENS     = 36,
		FUSE_OP_BMAP        = 37,
		FUSE_OP_MAX         = 38,
	};
}

#endif /* _INCLUDE__FUSE_PRIVATE_ */
