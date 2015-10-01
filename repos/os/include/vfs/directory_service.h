/*
 * \brief  Directory-service interface
 * \author Norman Feske
 * \date   2011-02-17
 */

/*
 * Copyright (C) 2011-2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__VFS__DIRECTORY_SERVICE_H_
#define _INCLUDE__VFS__DIRECTORY_SERVICE_H_

#include <vfs/types.h>

namespace Vfs {
	class Vfs_handle;
	struct Directory_service;
}


struct Vfs::Directory_service
{
	virtual Dataspace_capability dataspace(char const *path) = 0;
	virtual void release(char const *path, Dataspace_capability) = 0;


	enum General_error { ERR_FD_INVALID, NUM_GENERAL_ERRORS };


	/**********
	 ** Open **
	 **********/

	/**
	 * Flags of 'mode' argument of open syscall
	 */
	enum {
		OPEN_MODE_RDONLY  = 0,
		OPEN_MODE_WRONLY  = 1,
		OPEN_MODE_RDWR    = 2,
		OPEN_MODE_ACCMODE = 3,
		OPEN_MODE_CREATE  = 0x0800, /* libc O_EXCL */
	};

	enum Open_result
	{
		OPEN_ERR_UNACCESSIBLE,
		OPEN_ERR_NO_PERM,
		OPEN_ERR_EXISTS,
		OPEN_ERR_NAME_TOO_LONG,
		OPEN_ERR_NO_SPACE,
		OPEN_OK
	};

	virtual Open_result open(char const *path, unsigned mode, Vfs_handle **) = 0;


	/**********
	 ** Stat **
	 **********/

	/**
	 * These values are the same as in the FreeBSD libc
	 */
	enum {
		STAT_MODE_SYMLINK   = 0120000,
		STAT_MODE_FILE      = 0100000,
		STAT_MODE_DIRECTORY = 0040000,
		STAT_MODE_CHARDEV   = 0020000,
		STAT_MODE_BLOCKDEV  = 0060000,
	};

	struct Stat
	{
		file_size      size;
		unsigned       mode;
		unsigned       uid;
		unsigned       gid;
		unsigned long  inode;
		unsigned       device;
	};

	enum Stat_result { STAT_ERR_NO_ENTRY = NUM_GENERAL_ERRORS, STAT_OK };

	virtual Stat_result stat(char const *path, Stat &) = 0;


	/************
	 ** Dirent **
	 ************/

	enum Dirent_result { DIRENT_ERR_INVALID_PATH, DIRENT_OK };

	enum { DIRENT_MAX_NAME_LEN = 128 };

	enum Dirent_type {
		DIRENT_TYPE_FILE,
		DIRENT_TYPE_DIRECTORY,
		DIRENT_TYPE_FIFO,
		DIRENT_TYPE_CHARDEV,
		DIRENT_TYPE_BLOCKDEV,
		DIRENT_TYPE_SYMLINK,
		DIRENT_TYPE_END
	};

	struct Dirent
	{
		int         fileno;
		Dirent_type type;
		char        name[DIRENT_MAX_NAME_LEN];
	};

	virtual Dirent_result dirent(char const *path, file_offset index, Dirent &) = 0;


	/************
	 ** Unlink **
	 ************/

	enum Unlink_result { UNLINK_ERR_NO_ENTRY, UNLINK_ERR_NO_PERM, UNLINK_OK };

	virtual Unlink_result unlink(char const *path) = 0;


	/**************
	 ** Readlink **
	 **************/

	enum Readlink_result { READLINK_ERR_NO_ENTRY, READLINK_OK };

	virtual Readlink_result readlink(char const *path, char *buf,
	                                 file_size buf_size, file_size &out_len) = 0;


	/************
	 ** Rename **
	 ************/

	enum Rename_result { RENAME_ERR_NO_ENTRY, RENAME_ERR_CROSS_FS,
	                     RENAME_ERR_NO_PERM,  RENAME_OK };

	virtual Rename_result rename(char const *from, char const *to) = 0;


	/***********
	 ** Mkdir **
	 ***********/

	enum Mkdir_result { MKDIR_ERR_EXISTS,        MKDIR_ERR_NO_ENTRY,
	                    MKDIR_ERR_NO_SPACE,      MKDIR_ERR_NO_PERM,
	                    MKDIR_ERR_NAME_TOO_LONG, MKDIR_OK};

	virtual Mkdir_result mkdir(char const *path, unsigned mode) = 0;


	/*************
	 ** Symlink **
	 *************/

	enum Symlink_result { SYMLINK_ERR_EXISTS,        SYMLINK_ERR_NO_ENTRY,
	                      SYMLINK_ERR_NO_SPACE,      SYMLINK_ERR_NO_PERM,
	                      SYMLINK_ERR_NAME_TOO_LONG, SYMLINK_OK };

	virtual Symlink_result symlink(char const *from, char const *to) = 0;


	/**
	 * Return number of directory entries located at given path
	 */
	virtual file_size num_dirent(char const *path) = 0;

	virtual bool is_directory(char const *path) = 0;

	virtual char const *leaf_path(char const *path) = 0;
};

#endif /* _INCLUDE__VFS__DIRECTORY_SERVICE_H_ */
