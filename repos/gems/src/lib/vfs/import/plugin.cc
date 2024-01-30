/*
 * \brief  VFS content initialization/import plugin
 * \author Emery Hemingway
 * \date   2018-07-05
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <os/vfs.h>
#include <vfs/print.h>
#include <base/heap.h>

namespace Vfs_import {
	using namespace Vfs;
	class Flush_guard;
	class File_system;
	using Genode::Directory;
	using Genode::Root_directory;
}


/**
 * Utility to flush or sync a handle upon
 * leaving scope. Use with caution, syncing
 * may block for I/O signals.
 */
class Vfs_import::Flush_guard
{
	private:

		Vfs::Env::Io &_io;
		Vfs_handle   &_handle;

	public:

		Flush_guard(Vfs::Env::Io &io, Vfs_handle &handle)
		: _io(io), _handle(handle) { }

		~Flush_guard()
		{
			while (true) {
				if ((_handle.fs().queue_sync(&_handle))
				 && (_handle.fs().complete_sync(&_handle)
				  == Vfs::File_io_service::SYNC_OK))
					break;
				_io.commit_and_wait();
			}
		}
};


class Vfs_import::File_system : public Vfs::File_system
{
	private:

		/**
		 * XXX: A would-be temporary heap, but
		 * deconstructing a VFS is not supported.
		 */
		Genode::Heap _heap;

		enum { CREATE_IT = true };

		static void copy_symlink(Vfs::Env &env,
		                         Root_directory &src,
		                         Directory::Path const &path,
		                         Genode::Allocator &alloc,
		                         bool overwrite)
		{
			Directory::Path target = src.read_symlink(path);

			Vfs_handle *dst_handle = nullptr;
			auto res = env.root_dir().openlink(
				path.string(), true, &dst_handle, alloc);
			if (res == OPENLINK_ERR_NODE_ALREADY_EXISTS && overwrite) {
				res = env.root_dir().openlink(
					path.string(), false, &dst_handle, alloc);
			}
			if (res != OPENLINK_OK) {
				if (res != OPENLINK_ERR_NODE_ALREADY_EXISTS)
					Genode::warning("skipping copy of symlink ", path, ", ", res);
				return;
			}

			Vfs_handle::Guard guard(dst_handle);
			{
				Flush_guard flush(env.io(), *dst_handle);

				Const_byte_range_ptr const src { target.string(), target.length() };

				for (;;) {
					size_t out_count = 0;
					auto wres = dst_handle->fs().write(dst_handle, src, out_count);

					switch (wres) {
					case WRITE_ERR_WOULD_BLOCK:
						break;
					default:
						if (out_count < src.num_bytes) {
							Genode::error("failed to write symlink ", path, ", ", wres);
							env.root_dir().unlink(path.string());
						}
						return;
					}
				}
			}
		}

		static void copy_file(Vfs::Env &env,
		                      Root_directory &src,
		                      Directory::Path const &path,
		                      Genode::Allocator &alloc,
		                      bool overwrite)
		{
			using Genode::Readonly_file;
			using Genode::size_t;

			Readonly_file src_file(src, path);
			Vfs_handle *dst_handle = nullptr;

			enum {
				WRITE  = OPEN_MODE_WRONLY,
				CREATE = OPEN_MODE_WRONLY | OPEN_MODE_CREATE
			};

			auto res = env.root_dir().open(
				path.string(), CREATE , &dst_handle, alloc);
			if (res == OPEN_ERR_EXISTS && overwrite) {
				res = env.root_dir().open(
					path.string(), WRITE, &dst_handle, alloc);
			}
			if (res != OPEN_OK) {
				Genode::warning("skipping copy of file ", path, ", ", res);
				return;
			}

			dst_handle->fs().ftruncate(dst_handle, 0);

			char              buf[4096];
			Vfs_handle::Guard guard { dst_handle };
			Flush_guard       flush { env.io(), *dst_handle };
			Readonly_file::At at    { };

			while (true) {

				size_t const bytes_from_source =
					src_file.read(at, Genode::Byte_range_ptr(buf, sizeof(buf)));

				if (!bytes_from_source)
					break;

				bool write_error = false;

				size_t remaining_bytes = bytes_from_source;

				char const *src_ptr = buf;

				while (remaining_bytes > 0 && !write_error) {

					size_t out_count = 0;

					Const_byte_range_ptr const src { src_ptr, remaining_bytes };

					switch (dst_handle->fs().write(dst_handle, src, out_count)) {

					case WRITE_ERR_WOULD_BLOCK:
						env.io().commit_and_wait();
						break;

					case Write_result::WRITE_ERR_INVALID:
					case Write_result::WRITE_ERR_IO:
						env.root_dir().unlink(path.string());
						write_error = true;
						break;

					case WRITE_OK:
						out_count = min(remaining_bytes, out_count);
						remaining_bytes -= out_count;
						src_ptr         += out_count;
						at.value        += out_count;
						dst_handle->advance_seek(out_count);
						break;
					}
				}
				if (write_error)
					break;
			}
		}

		static void copy_dir(Vfs::Env &env,
		                     Root_directory &src,
		                     Directory::Path const &path,
		                     Genode::Allocator &alloc,
		                     bool overwrite)
		{
			{
				Vfs_handle *dir_handle = nullptr;
				env.root_dir().opendir(
					path.string(), CREATE_IT, &dir_handle, alloc);
				if (dir_handle)
					dir_handle->close();
			}

			{
				Directory dir(src, path);
				dir.for_each_entry([&] (Directory::Entry const &e) {
					auto entry_path = Directory::join(path, e.name());
					switch (e.type()) {
					case Dirent_type::TRANSACTIONAL_FILE:
					case Dirent_type::CONTINUOUS_FILE:
						copy_file(env, src, entry_path, alloc, overwrite);
						return;
					case Dirent_type::DIRECTORY:
						copy_dir(env, src, entry_path, alloc, overwrite);
						return;
					case Dirent_type::SYMLINK:
						copy_symlink(env, src, entry_path, alloc, overwrite);
						return;
					case Dirent_type::END:
						return;
					}
					Genode::warning("skipping copy of ", e);
				});
			}
		}

	public:

		File_system(Vfs::Env &env, Genode::Xml_node config)
		: _heap(env.env().pd(), env.env().rm())
		{
			bool overwrite = config.attribute_value("overwrite", false);

			Root_directory content(env.env(), _heap, config);
			copy_dir(env, content, Directory::Path(""), _heap, overwrite);
		}

		const char* type() override { return "import"; }

		/***********************
		 ** Directory service **
		 ***********************/

		Genode::Dataspace_capability dataspace(char const*) override {
			return Genode::Dataspace_capability(); }

		void release(char const*, Dataspace_capability) override { }

		Open_result open(const char*, unsigned, Vfs::Vfs_handle**, Genode::Allocator&) override {
			return Open_result::OPEN_ERR_UNACCESSIBLE; }

		Opendir_result opendir(char const*, bool,
	                           Vfs_handle**, Allocator&) override {
			return OPENDIR_ERR_LOOKUP_FAILED; }

		void close(Vfs::Vfs_handle*) override { }

		Stat_result stat(const char*, Vfs::Directory_service::Stat&) override {
			return STAT_ERR_NO_ENTRY; }

		Unlink_result unlink(const char*) override { return UNLINK_ERR_NO_ENTRY; }

		Rename_result rename(const char*, const char*) override {
			return RENAME_ERR_NO_ENTRY; }

		file_size num_dirent(const char*) override {
			return 0; }

		bool directory(char const*) override {
			return false; }

		const char* leaf_path(const char *) override {
			return nullptr; }

		/**********************
		 ** File I/O service **
		 **********************/

		Write_result write(Vfs_handle*, Const_byte_range_ptr const &, size_t &) override {
			return WRITE_ERR_INVALID; }

		Read_result complete_read(Vfs_handle*, Byte_range_ptr const &, size_t &) override {
			return READ_ERR_INVALID; }

		bool read_ready(Vfs_handle const &) const override {
			return true; }

		bool write_ready(Vfs_handle const &) const override {
			return true; }

		bool notify_read_ready(Vfs_handle*) override {
			return false; }

		Ftruncate_result ftruncate(Vfs_handle*, file_size) override {
			return FTRUNCATE_ERR_NO_PERM; }

		Sync_result complete_sync(Vfs_handle*) override {
			return SYNC_OK; }
};


extern "C" Vfs::File_system_factory *vfs_file_system_factory(void)
{
	struct Factory : Vfs::File_system_factory
	{
		Vfs::File_system *create(Vfs::Env &env, Genode::Xml_node config) override
		{
			return new (env.alloc())
				Vfs_import::File_system(env, config);
		}
	};

	static Factory f;
	return &f;
}
