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
	class Vfs_watch_handle;
	struct Directory_service;

	using Genode::Allocator;
}


struct Vfs::Directory_service : Interface
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

	virtual Opendir_result opendir(char const * /* path */, bool /* create */,
	                               Vfs_handle **, Allocator &)
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

	virtual Openlink_result openlink(char const * /* path */, bool /* create */,
	                                 Vfs_handle **, Allocator &)
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

	enum Watch_result
	{
		WATCH_ERR_UNACCESSIBLE,
		WATCH_ERR_STATIC,
		WATCH_ERR_OUT_OF_RAM,
		WATCH_ERR_OUT_OF_CAPS,
		WATCH_OK
	};

	/**
	 * Watch a file-system node for changes.
	 */
	virtual Watch_result watch(char const *path,
	                           Vfs_watch_handle**,
	                           Allocator&)
	{
		/* default implementation for static file-systems */
		return (leaf_path(path)) ? WATCH_ERR_STATIC : WATCH_ERR_UNACCESSIBLE;
	}

	virtual void close(Vfs_watch_handle *)
	{
		Genode::error("watch handle closed at invalid file-system");
		throw ~0;
	};

	/**********
	 ** Stat **
	 **********/

	struct Stat
	{
		file_size     size;
		Node_type     type;
		Node_rwx      rwx;
		unsigned long inode;
		unsigned long device;
		Timestamp     modification_time;
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

	enum class Dirent_type
	{
		END,
		DIRECTORY,
		SYMLINK,
		CONTINUOUS_FILE,
		TRANSACTIONAL_FILE,
	};

	struct Dirent
	{
		struct Name
		{
			enum { MAX_LEN = 128 };
			char buf[MAX_LEN] { };

			Name() { };
			Name(char const *name) { strncpy(buf, name, sizeof(buf)); }
		};

		unsigned long fileno;
		Dirent_type   type;
		Node_rwx      rwx;
		Name          name;

		/**
		 * Sanitize dirent members
		 *
		 * This method must be called after receiving a 'Dirent' as
		 * a plain data copy.
		 */
		void sanitize()
		{
			/* enforce null termination */
			name.buf[Name::MAX_LEN - 1] = 0;
		}
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

	/**
	 * Return leaf path or nullptr if the path does not exist
	 */
	virtual char const *leaf_path(char const *path) = 0;
};

#endif /* _INCLUDE__VFS__DIRECTORY_SERVICE_H_ */
