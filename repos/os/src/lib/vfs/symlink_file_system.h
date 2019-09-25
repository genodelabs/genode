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

		typedef Genode::String<MAX_PATH_LEN> Target;

		Target const _target;

		struct Symlink_handle final : Single_vfs_handle
		{
			Target const &_target;

			Symlink_handle(Directory_service &ds,
			               File_io_service   &fs,
			               Genode::Allocator &alloc,
			               Target const &target)
			: Single_vfs_handle(ds, fs, alloc, 0), _target(target)
			{ }


			Read_result read(char *dst, file_size count,
			                 file_size &out_count) override
			{
				auto n = min(count, _target.length());
				strncpy(dst, _target.string(), n);
				out_count = n - 1;
				return READ_OK;
			}

			Write_result write(char const*, file_size,
			                   file_size&) override {
				return WRITE_ERR_INVALID; }

			bool read_ready() override { return true; }
		};

	public:

		Symlink_file_system(Vfs::Env&, Genode::Xml_node config)
		:
			Single_file_system(Node_type::SYMLINK, "symlink",
			                   Node_rwx::rw(), config),
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
				*out_handle = new (alloc) Symlink_handle(*this, *this, alloc, _target);
				return OPENLINK_OK;
			}
			catch (Genode::Out_of_ram)  { return OPENLINK_ERR_OUT_OF_RAM; }
			catch (Genode::Out_of_caps) { return OPENLINK_ERR_OUT_OF_CAPS; }
		}
};

#endif /* _INCLUDE__VFS__SYMLINK_FILE_SYSTEM_H_ */
