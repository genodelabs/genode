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

		Symlink_file_system(Vfs::Env&, Genode::Xml_node config)
		:
			Single_file_system(NODE_TYPE_SYMLINK, "symlink", config),
			_target(config.attribute_value("target", Target()))
		{ }

		static char const *name()   { return "symlink"; }
		char const *type() override { return "symlink"; }

		/*********************************
		 ** Directory-service interface **
		 *********************************/

		Open_result open(char const *, unsigned, Vfs_handle **, Allocator&) override {
			return OPEN_ERR_UNACCESSIBLE; }

		Openlink_result openlink(char const *path, bool create,
		                         Vfs_handle **out_handle, Allocator &alloc) override
		{
			if (!_single_file(path))
				return OPENLINK_ERR_LOOKUP_FAILED;

			if (create)
				return OPENLINK_ERR_NODE_ALREADY_EXISTS;

			try {
				*out_handle = new (alloc) Vfs_handle(*this, *this, alloc, 0);
				return OPENLINK_OK;
			}
			catch (Genode::Out_of_ram)  { return OPENLINK_ERR_OUT_OF_RAM; }
			catch (Genode::Out_of_caps) { return OPENLINK_ERR_OUT_OF_CAPS; }
		}

		void close(Vfs_handle *vfs_handle) override
		{
			if (!vfs_handle)
				return;

			destroy(vfs_handle->alloc(), vfs_handle);
		}


		/********************************
		 ** File I/O service interface **
		 ********************************/

		Write_result write(Vfs_handle *, char const *,
		                   file_size, file_size &) override {
			return WRITE_ERR_INVALID; }

		Read_result complete_read(Vfs_handle *, char *buf, file_size buf_len,
		                          file_size &out_len) override
		{
			out_len = min(buf_len, (file_size)_target.length()-1);
			memcpy(buf, _target.string(), out_len);
			if (out_len < buf_len)
				buf[out_len] = '\0';
			return READ_OK;
		}

		bool read_ready(Vfs_handle *) override { return false; }

		Ftruncate_result ftruncate(Vfs_handle *, file_size) override {
			return FTRUNCATE_ERR_NO_PERM; }
};

#endif /* _INCLUDE__VFS__SYMLINK_FILE_SYSTEM_H_ */
