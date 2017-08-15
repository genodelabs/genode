/*
 * \brief  Directory-service interface
 * \author Norman Feske
 * \date   2011-02-17
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__VFS__DIRECTORY_SERVICE_H_
#define _INCLUDE__VFS__DIRECTORY_SERVICE_H_

#include <vfs/types.h>

namespace Vfs {
	class Vfs_handle;
	struct Directory_service;

	using Genode::Allocator;
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
		OPEN_ERR_OUT_OF_RAM,
		OPEN_ERR_OUT_OF_CAPS,
		OPEN_OK
	};

	virtual Open_result open(char const  *path,
	                         unsigned     mode,
	                         Vfs_handle **handle,
	                         Allocator   &alloc) = 0;

	enum Opendir_result
	{
		OPENDIR_ERR_LOOKUP_FAILED,
		OPENDIR_ERR_NAME_TOO_LONG,
		OPENDIR_ERR_NODE_ALREADY_EXISTS,
		OPENDIR_ERR_NO_SPACE,
		OPENDIR_ERR_OUT_OF_RAM,
		OPENDIR_ERR_OUT_OF_CAPS,
		OPENDIR_ERR_PERMISSION_DENIED,
		OPENDIR_OK
	};

	virtual Opendir_result opendir(char const *path, bool create,
	                               Vfs_handle **handle, Allocator &alloc)
	{
		return OPENDIR_ERR_LOOKUP_FAILED;
	}

	enum Openlink_result
	{
		OPENLINK_ERR_LOOKUP_FAILED,
		OPENLINK_ERR_NAME_TOO_LONG,
		OPENLINK_ERR_NODE_ALREADY_EXISTS,
		OPENLINK_ERR_NO_SPACE,
		OPENLINK_ERR_OUT_OF_RAM,
		OPENLINK_ERR_OUT_OF_CAPS,
		OPENLINK_ERR_PERMISSION_DENIED,
		OPENLINK_OK
	};

	virtual Openlink_result openlink(char const *path, bool create,
	                                 Vfs_handle **handle, Allocator &alloc)
	{
		return OPENLINK_ERR_PERMISSION_DENIED;
	}

	/**
	 * Close handle resources and deallocate handle
	 *
	 * Note: it might be necessary to call 'sync()' before 'close()'
	 *       to ensure that previously written data has been completely
	 *       processed.
	 */
	virtual void close(Vfs_handle *handle) = 0;


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
		file_size     size   = 0;
		unsigned      mode   = 0;
		unsigned      uid    = 0;
		unsigned      gid    = 0;
		unsigned long inode  = 0;
		unsigned long device = 0;
	};

	enum Stat_result { STAT_ERR_NO_ENTRY = NUM_GENERAL_ERRORS,
	                   STAT_ERR_NO_PERM, STAT_OK };

	/*
	 * Note: it might be necessary to call 'sync()' before 'stat()'
	 *       to get the correct file size.
	 */
	virtual Stat_result stat(char const *path, Stat &) = 0;


	/************
	 ** Dirent **
	 ************/

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
		unsigned long fileno                    = 0;
		Dirent_type   type                      = DIRENT_TYPE_END;
		char          name[DIRENT_MAX_NAME_LEN] = { 0 };
	};


	/************
	 ** Unlink **
	 ************/

	enum Unlink_result { UNLINK_ERR_NO_ENTRY,  UNLINK_ERR_NO_PERM,
	                     UNLINK_ERR_NOT_EMPTY, UNLINK_OK };

	virtual Unlink_result unlink(char const *path) = 0;


	/************
	 ** Rename **
	 ************/

	enum Rename_result { RENAME_ERR_NO_ENTRY, RENAME_ERR_CROSS_FS,
	                     RENAME_ERR_NO_PERM,  RENAME_OK };

	virtual Rename_result rename(char const *from, char const *to) = 0;


	/**
	 * Return number of directory entries located at given path
	 */
	virtual file_size num_dirent(char const *path) = 0;

	virtual bool directory(char const *path) = 0;

	virtual char const *leaf_path(char const *path) = 0;
};

#endif /* _INCLUDE__VFS__DIRECTORY_SERVICE_H_ */
