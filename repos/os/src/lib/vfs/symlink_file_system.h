/*
 * \brief  Symlink filesystem
 * \author Norman Feske
 * \date   2015-08-21
 *
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__VFS__SYMLINK_FILE_SYSTEM_H_
#define _INCLUDE__VFS__SYMLINK_FILE_SYSTEM_H_

#include <vfs/single_file_system.h>

namespace Vfs { class Symlink_file_system; }


class Vfs::Symlink_file_system : public Single_file_system
{
	private:

		enum { FILENAME_MAX_LEN = 64 };
		typedef Genode::String<MAX_PATH_LEN> Target;

		Target const _target;

	public:

		Symlink_file_system(Genode::Env&,
		                    Genode::Allocator&,
		                    Genode::Xml_node config,
		                    Io_response_handler&)
		:
			Single_file_system(NODE_TYPE_SYMLINK, "symlink", config),
			_target(config.attribute_value("target", Target()))
		{ }

		static char const *name()   { return "symlink"; }
		char const *type() override { return "symlink"; }

		/*********************************
		 ** Directory-service interface **
		 *********************************/

		Symlink_result symlink(char const *from, char const *to) override {
			return SYMLINK_ERR_EXISTS; }

		Readlink_result readlink(char const *path,
		                         char       *buf,
		                         file_size   buf_len,
		                         file_size  &out_len) override
		{
			if (!_single_file(path))
				return READLINK_ERR_NO_ENTRY;
			out_len = min(buf_len, (file_size)_target.length()-1);
			memcpy(buf, _target.string(), out_len);
			if (out_len < buf_len)
				buf[out_len] = '\0';
			return READLINK_OK;
		}

		Open_result open(char const *, unsigned, Vfs_handle **out_handle,
		                 Allocator&) override {
			return OPEN_ERR_UNACCESSIBLE; }

		void close(Vfs_handle *) override { }


		/********************************
		 ** File I/O service interface **
		 ********************************/

		Write_result write(Vfs_handle *handle, char const *, file_size,
		                   file_size &) override {
			return WRITE_ERR_INVALID; }

		Read_result read(Vfs_handle *, char *, file_size, file_size &) override {
			return READ_ERR_INVALID; }

		bool read_ready(Vfs_handle *) override { return false; }

		Ftruncate_result ftruncate(Vfs_handle *, file_size) override {
			return FTRUNCATE_ERR_NO_PERM; }
};

#endif /* _INCLUDE__VFS__SYMLINK_FILE_SYSTEM_H_ */
