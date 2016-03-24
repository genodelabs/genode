/*
 * \brief  Symlink filesystem
 * \author Norman Feske
 * \date   2015-08-21
 *
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__VFS__SYMLINK_FILE_SYSTEM_H_
#define _INCLUDE__VFS__SYMLINK_FILE_SYSTEM_H_

#include <vfs/file_system.h>

namespace Vfs { class Symlink_file_system; }


class Vfs::Symlink_file_system : public File_system
{
	private:

		enum { FILENAME_MAX_LEN = 64 };

		char _target[MAX_PATH_LEN];
		char _filename[FILENAME_MAX_LEN];

		bool _is_root(const char *path)
		{
			return (strcmp(path, "") == 0) || (strcmp(path, "/") == 0);
		}

		bool _is_single_file(const char *path)
		{
			return (strlen(path) == (strlen(_filename) + 1)) &&
			       (strcmp(&path[1], _filename) == 0);
		}

	public:

		Symlink_file_system(Xml_node config)
		{
			try {
				config.attribute("name").value(_filename, sizeof(_filename));
				config.attribute("target").value(_target, sizeof(_target));
			} catch (...) { }

		}

		static char const *name() { return "symlink"; }


		/*********************************
		 ** Directory-service interface **
		 *********************************/

		Symlink_result symlink(char const *from, char const *to) override {
			return SYMLINK_ERR_EXISTS; }

		Readlink_result readlink(char const *path,
		                         char       *buf,
		                         file_size   buf_size,
		                         file_size  &out_len) override
		{
			if (!_is_single_file(path))
				return READLINK_ERR_NO_ENTRY;
			out_len = min(buf_size, file_size(sizeof(_target)));
			strncpy(buf, _target, out_len);
			return READLINK_OK;
		}

		Stat_result stat(char const *path, Stat &out) override
		{
			out = { 0, 0, 0, 0, 0, 0 };

			if (_is_root(path)) {
				out.mode = STAT_MODE_DIRECTORY;

			} else if (_is_single_file(path)) {
				out.mode = STAT_MODE_SYMLINK;
			} else {
				return STAT_ERR_NO_ENTRY;
			}
			return STAT_OK;
		}

		file_size num_dirent(char const *path) override
		{
			if (_is_root(path))
				return 1;
			else
				return 0;
		}

		bool is_directory(char const *path) override
		{
			if (_is_root(path))
				return true;

			return false;
		}

		char const *leaf_path(char const *path) override
		{
			return _is_single_file(path) ? path : 0;
		}

		Dirent_result dirent(char const *path, file_offset index, Dirent &out) override
		{
			if (!_is_root(path))
				return DIRENT_ERR_INVALID_PATH;

			out.fileno = 1;
			if (index == 0) {
				out.type = DIRENT_TYPE_SYMLINK;
				strncpy(out.name, _filename, sizeof(out.name));
			} else {
				out.type = DIRENT_TYPE_END;
				out.name[0] = '\0';
			}

			return DIRENT_OK;
		}

		Dataspace_capability dataspace(char const *path) override {
			return Dataspace_capability(); }

		void release(char const *path, Dataspace_capability ds_cap) override { }

		Open_result open(char const *path, unsigned,
		                 Vfs_handle **out_handle) override {
			return OPEN_ERR_UNACCESSIBLE; }

		Unlink_result unlink(char const *) override {
			return UNLINK_ERR_NO_PERM; }

		Rename_result rename(char const *from, char const *to) override
		{
			if (_is_single_file(from) || _is_single_file(to))
				return RENAME_ERR_NO_PERM;
			return RENAME_ERR_NO_ENTRY;
		}

		Mkdir_result mkdir(char const *, unsigned) override {
			return MKDIR_ERR_NO_PERM; }


		/********************************
		 ** File I/O service interface **
		 ********************************/

		Write_result write(Vfs_handle *handle, char const *, file_size,
		                   file_size &) override
		{
			return WRITE_ERR_INVALID;
		}

		Read_result read(Vfs_handle *, char *, file_size, file_size &) override
		{
			return READ_ERR_INVALID;
		}

		Ftruncate_result ftruncate(Vfs_handle *, file_size) override
		{
			return FTRUNCATE_ERR_NO_PERM;
		}

};

#endif /* _INCLUDE__VFS__SYMLINK_FILE_SYSTEM_H_ */