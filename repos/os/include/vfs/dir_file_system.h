/*
 * \brief  Directory file system
 * \author Norman Feske
 * \author Emery Hemingway
 * \author Christian Helmuth
 * \date   2012-04-23
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__VFS__DIR_FILE_SYSTEM_H_
#define _INCLUDE__VFS__DIR_FILE_SYSTEM_H_

#include <base/registry.h>
#include <vfs/file_system_factory.h>
#include <vfs/vfs_handle.h>


namespace Vfs { class Dir_file_system; }


class Vfs::Dir_file_system : public File_system
{
	public:

		enum { MAX_NAME_LEN = 128 };

		struct Root { };

	private:

		/**
		 * This instance is the root of VFS
		 *
		 * Additionally, the root has an empty _name.
		 */
		bool _vfs_root;

		struct Dir_vfs_handle : Vfs_handle
		{
			struct Subdir_handle_element;

			typedef Genode::Registry<Subdir_handle_element> Subdir_handle_registry;

			struct Subdir_handle_element : Subdir_handle_registry::Element
			{
				Vfs_handle &vfs_handle;
				Subdir_handle_element(Subdir_handle_registry &registry,
				                      Vfs_handle &vfs_handle)
				: Subdir_handle_registry::Element(registry, *this),
				  vfs_handle(vfs_handle) { }
			};

			Absolute_path             path;
			Vfs_handle               *queued_read_handle { nullptr };
			Subdir_handle_registry    subdir_handle_registry;

			Dir_vfs_handle(Directory_service &ds,
			               File_io_service   &fs,
			               Genode::Allocator &alloc,
			               char const *path)
			: Vfs_handle(ds, fs, alloc, 0),
			  path(path) { }

			~Dir_vfs_handle()
			{
				/* close all sub-handles */
				auto f = [&] (Subdir_handle_element &e) {
					e.vfs_handle.ds().close(&e.vfs_handle);
					destroy(alloc(), &e);
				};
				subdir_handle_registry.for_each(f);
			}
		};

		/* pointer to first child file system */
		File_system *_first_file_system;

		/* add new file system to the list of children */
		void _append_file_system(File_system *fs)
		{
			if (!_first_file_system) {
				_first_file_system = fs;
				return;
			}

			File_system *curr = _first_file_system;
			while (curr->next)
				curr = curr->next;

			curr->next = fs;
		}

		/**
		 * Directory name
		 */
		char _name[MAX_NAME_LEN];

		/**
		 * Returns if path corresponds to top directory of file system
		 */
		bool _top_dir(char const *path) const {	return strcmp(path, "/") == 0; }

		/**
		 * Perform operation on a file system
		 *
		 * \param fn  functor that takes a file-system reference and
		 *            the path as arguments
		 */
		template <typename RES, typename FN>
		RES _dir_op(RES const no_entry, RES const no_perm, RES const ok,
		            char const *path, FN const &fn)
		{
			path = _sub_path(path);

			/* path does not match directory name */
			if (!path)
				return no_entry;

			/*
			 * Prevent operation if path equals directory name defined
			 * via the static VFS configuration.
			 */
			if (strlen(path) == 0)
				return no_perm;

			/*
			 * If any of the sub file systems returns a permission error and
			 * there exists no sub file system that takes the request, we
			 * return the permission error.
			 */
			bool permission_denied = false;

			/*
			 * Keep the most meaningful error code. When using stacked file
			 * systems, most child file systems will eventually return no
			 * entry (or leave the error code unchanged). If any of those
			 * file systems has anything more interesting to tell, return
			 * this information after all file systems have been tried and
			 * none could handle the request.
			 */
			RES error = ok;

			/*
			 * The given path refers to at least one of our sub directories.
			 * Propagate the request into all of our file systems. If at least
			 * one operation succeeds, we return success.
			 */
			for (File_system *fs = _first_file_system; fs; fs = fs->next) {

				RES const err = fn(*fs, path);

				if (err == ok)
					return err;

				if (err != no_entry && err != no_perm) {
					error = err;
				}

				if (err == no_perm)
					permission_denied = true;
			}

			/* none of our file systems could successfully operate on the path */
			return error != ok ? error : permission_denied ? no_perm : no_entry;
		}

		/**
		 * Return portion of the path after the element corresponding to
		 * the current directory.
		 */
		char const *_sub_path(char const *path) const
		{
			/* do not strip anything from the path when we are root */
			if (_vfs_root)
				return path;

			if (_top_dir(path))
				return path;

			/* skip heading slash in path if present */
			if (path[0] == '/')
				path++;

			Genode::size_t const name_len = strlen(_name);
			if (strcmp(path, _name, name_len) != 0)
				return 0;
			path += name_len;

			/*
			 * The first characters of the first path element are equal to
			 * the current directory name. Let's check if the length of the
			 * first path element matches the name length.
			 */
			if (*path != 0 && *path != '/')
				return 0;

			return path;
		}

		/*
		 * Accumulate number of directory entries that match in any of
		 * our sub file systems.
		 */
		file_size _sum_dirents_of_file_systems(char const *path)
		{
			file_size cnt = 0;
			for (File_system *fs = _first_file_system; fs; fs = fs->next) {
				cnt += fs->num_dirent(path);
			}
			return cnt;
		}

		bool _queue_read_of_file_systems(Dir_vfs_handle *dir_vfs_handle)
		{
			bool result = true;

			dir_vfs_handle->queued_read_handle = nullptr;

			file_offset index = dir_vfs_handle->seek() / sizeof(Dirent);

			char const *sub_path = _sub_path(dir_vfs_handle->path.base());

			if (strlen(sub_path) == 0)
				sub_path = "/";

			/* base of composite directory index */
			int base = 0;

			auto f = [&] (Dir_vfs_handle::Subdir_handle_element &handle_element) {
				if (dir_vfs_handle->queued_read_handle) return; /* skip through */

				Vfs_handle &vfs_handle = handle_element.vfs_handle;

				/*
				 * Determine number of matching directory entries within
				 * the current file system.
				 */
				int const fs_num_dirent = vfs_handle.ds().num_dirent(sub_path);

				/*
				 * Query directory entry if index lies with the file
				 * system.
				 */
				if (index - base < fs_num_dirent) {
					/* set this handle to be used for read completion */
					dir_vfs_handle->queued_read_handle = &vfs_handle;

					/* seek to file-system local index */
					index = index - base;
					vfs_handle.seek(index * sizeof(Dirent));

					/* forward the handle context */
					vfs_handle.context = dir_vfs_handle->context;

					result = vfs_handle.fs().queue_read(&vfs_handle, sizeof(Dirent));
				}

				/* adjust base index for next file system */
				base += fs_num_dirent;
			};

			dir_vfs_handle->subdir_handle_registry.for_each(f);

			return result;
		}

		Read_result _complete_read_of_file_systems(Dir_vfs_handle *dir_vfs_handle,
		                                           char *dst, file_size count,
		                                           file_size &out_count)
		{
			if (!dir_vfs_handle->queued_read_handle) {

				/*
				 * no fs was found for the given index or
				 * fs->opendir() failed
				 */

				if (count < sizeof(Dirent))
					return READ_ERR_INVALID;

				Dirent *dirent = (Dirent*)dst;
				*dirent = Dirent();

				out_count = sizeof(Dirent);

				return READ_OK;
			}

			Read_result result = dir_vfs_handle->queued_read_handle->fs().
			                     complete_read(dir_vfs_handle->queued_read_handle,
			                                   dst, count, out_count);

			if (result == READ_QUEUED)
				return result;

			dir_vfs_handle->queued_read_handle = nullptr;

			return result;
		}

	public:

		Dir_file_system(Genode::Env         &env,
		                Genode::Allocator   &alloc,
		                Genode::Xml_node     node,
		                Io_response_handler &io_handler,
		                File_system_factory &fs_factory)
		:
			_vfs_root(false),
			_first_file_system(0)
		{
			using namespace Genode;

			/* remember directory name */
			if (node.has_type("fstab") || node.has_type("vfs"))
				_name[0] = 0;
			else
				node.attribute("name").value(_name, sizeof(_name));

			for (unsigned i = 0; i < node.num_sub_nodes(); i++) {

				Xml_node sub_node = node.sub_node(i);

				/* traverse into <dir> nodes */
				if (sub_node.has_type("dir")) {
					_append_file_system(new (alloc)
						Dir_file_system(env, alloc, sub_node, io_handler, fs_factory));
					continue;
				}

				File_system *fs = fs_factory.create(env, alloc, sub_node, io_handler);
				if (fs) {
					_append_file_system(fs);
					continue;
				}

				Genode::error("failed to create <", sub_node.type(), "> VFS node");
				try {
					String<64> value;
					for (unsigned i = 0; i < 16; ++i) {
						Xml_attribute attr = sub_node.attribute(i);
						attr.value(&value);

						Genode::error("\t", attr.name(), "=\"", value, "\"");
					}
				} catch (Xml_node::Nonexistent_attribute) { }
			}
		}

		Dir_file_system(Genode::Env         &env,
		                Genode::Allocator   &alloc,
		                Genode::Xml_node     node,
		                Io_response_handler &io_handler,
		                File_system_factory &fs_factory,
		                Dir_file_system::Root)
		:
			Dir_file_system(env, alloc, node, io_handler, fs_factory)
			{ _vfs_root = true; }

		/*********************************
		 ** Directory-service interface **
		 *********************************/

		Dataspace_capability dataspace(char const *path) override
		{
			path = _sub_path(path);
			if (!path)
				return Dataspace_capability();

			/*
			 * Query sub file systems for dataspace using the path local to
			 * the respective file system
			 */
			File_system *fs = _first_file_system;
			for (; fs; fs = fs->next) {
				Dataspace_capability ds = fs->dataspace(path);
				if (ds.valid())
					return ds;
			}

			return Dataspace_capability();
		}

		void release(char const *path, Dataspace_capability ds_cap) override
		{
			path = _sub_path(path);
			if (!path)
				return;

			for (File_system *fs = _first_file_system; fs; fs = fs->next)
				fs->release(path, ds_cap);
		}

		Stat_result stat(char const *path, Stat &out) override
		{
			path = _sub_path(path);

			/* path does not match directory name */
			if (!path)
				return STAT_ERR_NO_ENTRY;

			/*
			 * If path equals directory name, return information about the
			 * current directory.
			 */
			if (strlen(path) == 0 || _top_dir(path)) {
				out.size   = 0;
				out.mode   = STAT_MODE_DIRECTORY | 0755;
				out.uid    = 0;
				out.gid    = 0;
				out.inode  = 1;
				out.device = (Genode::addr_t)this;
				return STAT_OK;
			}

			/*
			 * The given path refers to one of our sub directories.
			 * Propagate the request into our file systems.
			 */
			for (File_system *fs = _first_file_system; fs; fs = fs->next) {

				Stat_result const err = fs->stat(path, out);

				if (err == STAT_OK)
					return err;

				if (err != STAT_ERR_NO_ENTRY)
					return err;
			}

			/* none of our file systems felt responsible for the path */
			return STAT_ERR_NO_ENTRY;
		}

		file_size num_dirent(char const *path) override
		{
			if (_vfs_root) {
				return _sum_dirents_of_file_systems(path);

			} else {

				if (_top_dir(path))
					return 1;

				/*
				 * The path contains at least one element. Remove current
				 * element from path.
				 */
				path = _sub_path(path);

				/*
				 * If the resulting 'path' is non-null, the path lies
				 * within our tree. In this case, determine the sum of
				 * matching dirents of all our file systems. Otherwise,
				 * the specified path lies outside our directory node.
				 */
				return path ? _sum_dirents_of_file_systems(*path ? path : "/") : 0;
			}
		}

		/**
		 * Return true if specified path is a directory
		 */
		bool directory(char const *path) override
		{
			if (_top_dir(path))
				return true;

			path = _sub_path(path);

			if (!path)
				return false;

			if (strlen(path) == 0)
				return true;

			for (File_system *fs = _first_file_system; fs; fs = fs->next)
				if (fs->directory(path))
					return true;

			return false;
		}

		/**
		 * Return true if specified path is a directory
		 *
		 * \noapi
		 * \deprecated  use 'directory instead
		 */
		bool is_directory(char const *path) { return directory(path); }

		char const *leaf_path(char const *path) override
		{
			path = _sub_path(path);
			if (!path)
				return 0;

			if (strlen(path) == 0)
				return path;

			for (File_system *fs = _first_file_system; fs; fs = fs->next) {
				char const *leaf_path = fs->leaf_path(path);
				if (leaf_path)
					return leaf_path;
			}

			return 0;
		}

		Open_result open(char const  *path,
		                 unsigned     mode,
		                 Vfs_handle **out_handle,
		                 Allocator   &alloc) override
		{
			/*
			 * If 'path' is a directory, we create a 'Vfs_handle'
			 * for the root directory so that subsequent 'dirent' calls
			 * are subjected to the stacked file-system layout.
			 */
			if (directory(path)) {
				*out_handle = new (alloc) Vfs_handle(*this, *this, alloc, 0);
				return OPEN_OK;
			}

			/*
			 * If 'path' refers to a non-directory node, create a
			 * 'Vfs_handle' local to the file system that provides the
			 * file.
			 */

			path = _sub_path(path);

			/* check if path does not match directory name */
			if (!path)
				return OPEN_ERR_UNACCESSIBLE;

			/* path equals directory name */
			if (strlen(path) == 0) {
				*out_handle = new (alloc) Vfs_handle(*this, *this, alloc, 0);
				return OPEN_OK;
			}

			/* path refers to any of our sub file systems */
			for (File_system *fs = _first_file_system; fs; fs = fs->next) {

				Open_result const err = fs->open(path, mode, out_handle, alloc);
				switch (err) {
				case OPEN_ERR_UNACCESSIBLE:
					continue;
				default:
					return err;
				}
			}

			/* path does not match any existing file or directory */
			return OPEN_ERR_UNACCESSIBLE;
		}

		/**
		 * Call 'opendir()' on each file system and store handles in
		 * a registry.
		 */
		Opendir_result open_composite_dirs(char const *sub_path,
		                                   Dir_vfs_handle &dir_vfs_handle)
		{
			try {
				for (File_system *fs = _first_file_system; fs; fs = fs->next) {
					Vfs_handle *sub_dir_handle = nullptr;

					Opendir_result r = fs->opendir(
						sub_path, false, &sub_dir_handle, dir_vfs_handle.alloc());

					switch (r) {
					case OPENDIR_OK:
						break;
					case OPENDIR_ERR_LOOKUP_FAILED:
					default:
						continue;
					}

					new (dir_vfs_handle.alloc())
						Dir_vfs_handle::Subdir_handle_element(
							dir_vfs_handle.subdir_handle_registry, *sub_dir_handle);
				}
			}
			catch (Genode::Out_of_ram)  { return OPENDIR_ERR_OUT_OF_RAM; }
			catch (Genode::Out_of_caps) { return OPENDIR_ERR_OUT_OF_CAPS; }

			return OPENDIR_OK;
		}

		Opendir_result opendir(char const *path, bool create,
		                       Vfs_handle **out_handle, Allocator &alloc) override
		{
			Opendir_result result = OPENDIR_OK;

			if (_top_dir(path)) {
				if (create)
					return OPENDIR_ERR_PERMISSION_DENIED;

				/*
				 * opendir with '/' (called from 'open_composite_dirs' returns handle
				 * only, VFS root additionally calls 'open_composite_dirs' in order to
				 * open its file systems
				 */
				Dir_vfs_handle *root_handle = new (alloc)
					Dir_vfs_handle(*this, *this, alloc, path);

				/* the VFS root may contain more file systems */
				if (_vfs_root)
					result = open_composite_dirs("/", *root_handle);

				if (result == OPENDIR_OK) {
					*out_handle = root_handle;
				} else {
					/* close the root handle and the rest will follow */
					close(root_handle);
				}
				return result;
			}

			char const *sub_path = _sub_path(path);

			if (!sub_path)
				return OPENDIR_ERR_LOOKUP_FAILED;

			if (create) {
				auto opendir_fn = [&] (File_system &fs, char const *path)
				{
					Vfs_handle *tmp_handle;
					Opendir_result opendir_result =
						fs.opendir(path, true, &tmp_handle, alloc);
					if (opendir_result == OPENDIR_OK)
						fs.close(tmp_handle);
					return opendir_result; /* return from lambda */
				};

				Opendir_result opendir_result =
					_dir_op(OPENDIR_ERR_LOOKUP_FAILED,
				            OPENDIR_ERR_PERMISSION_DENIED,
				            OPENDIR_OK,
				            path, opendir_fn);

				if (opendir_result != OPENDIR_OK)
					return opendir_result;
			}

			Dir_vfs_handle *dir_vfs_handle = new (alloc)
				Dir_vfs_handle(*this, *this, alloc, path);

			/* path equals "/" (for reading the name of this directory) */
			if (strlen(sub_path) == 0)
				sub_path = "/";

			result = open_composite_dirs(sub_path, *dir_vfs_handle);
			if (result == OPENDIR_OK) {
				*out_handle = dir_vfs_handle;
			} else {
				/* close the master handle and the rest will follow */
				close(dir_vfs_handle);
			}
			return result;
		}

		Openlink_result openlink(char const *path, bool create,
		                         Vfs_handle **out_handle,
		                         Allocator &alloc) override
		{
			auto openlink_fn = [&] (File_system &fs, char const *path)
			{
				return fs.openlink(path, create, out_handle, alloc);
			};

			return _dir_op(OPENLINK_ERR_LOOKUP_FAILED,
			               OPENLINK_ERR_PERMISSION_DENIED,
			               OPENLINK_OK,
			               path, openlink_fn);
		}

		void close(Vfs_handle *handle) override
		{
			if (handle && (&handle->ds() == this))
				destroy(handle->alloc(), handle);
		}

		Unlink_result unlink(char const *path) override
		{
			auto unlink_fn = [] (File_system &fs, char const *path)
			{
				return fs.unlink(path);
			};

			return _dir_op(UNLINK_ERR_NO_ENTRY, UNLINK_ERR_NO_PERM, UNLINK_OK,
			               path, unlink_fn);
		}

		Rename_result rename(char const *from_path, char const *to_path) override
		{
			from_path = _sub_path(from_path);
			to_path = _sub_path(to_path);

			/* path does not match directory name */
			if (!from_path)
				return RENAME_ERR_NO_ENTRY;

			/*
			 * Cannot rename a path in the static VFS configuration.
			 */
			if (strlen(from_path) == 0)
				return RENAME_ERR_NO_PERM;

			/*
			 * Check if destination path resides within the same file
			 * system instance as the source path.
			 */
			if (!to_path)
				return RENAME_ERR_CROSS_FS;

			Rename_result final = RENAME_ERR_NO_ENTRY;
			for (File_system *fs = _first_file_system; fs; fs = fs->next) {
				switch (fs->rename(from_path, to_path)) {
				case RENAME_OK:           return RENAME_OK;
				case RENAME_ERR_NO_ENTRY: continue;
				case RENAME_ERR_NO_PERM:  return RENAME_ERR_NO_PERM;
				case RENAME_ERR_CROSS_FS: final = RENAME_ERR_CROSS_FS;
				}
			}
			return final;
		}

		/***************************
		 ** File_system interface **
		 ***************************/

		char const *name() const    { return "dir"; }
		char const *type() override { return "dir"; }

		void apply_config(Genode::Xml_node const &node) override
		{
			using namespace Genode;

			File_system *curr = _first_file_system;
			for (unsigned i = 0; i < node.num_sub_nodes(); i++, curr = curr->next) {
				Xml_node const &sub_node = node.sub_node(i);

				/* check if type of XML node matches current file-system type */
				if (sub_node.has_type(curr->type()) == false) {
					Genode::error("VFS config update failed (node type '",
					               sub_node.type(), "' != fs type '", curr->type(),"')");
					return;
				}

				curr->apply_config(node.sub_node(i));
			}
		}


		/********************************
		 ** File I/O service interface **
		 ********************************/

		Write_result write(Vfs_handle *handle, char const *, file_size,
		                   file_size &) override
		{
			return WRITE_ERR_INVALID;
		}

		bool queue_read(Vfs_handle *vfs_handle, file_size count) override
		{
			Dir_vfs_handle *dir_vfs_handle =
				static_cast<Dir_vfs_handle*>(vfs_handle);

			if (_vfs_root)
				return _queue_read_of_file_systems(dir_vfs_handle);

			if (_top_dir(dir_vfs_handle->path.base()))
				return true;

			return _queue_read_of_file_systems(dir_vfs_handle);
		}

		Read_result complete_read(Vfs_handle *vfs_handle,
		                          char *dst, file_size count,
		                          file_size &out_count) override
		{
			out_count = 0;

			if (count < sizeof(Dirent))
				return READ_ERR_INVALID;

			Dir_vfs_handle *dir_vfs_handle =
				static_cast<Dir_vfs_handle*>(vfs_handle);

			if (_vfs_root)
				return _complete_read_of_file_systems(dir_vfs_handle, dst, count, out_count);

			if (_top_dir(dir_vfs_handle->path.base())) {
				Dirent *dirent = (Dirent*)dst;
				file_offset index = vfs_handle->seek() / sizeof(Dirent);

				if (index == 0) {
					strncpy(dirent->name, _name, sizeof(dirent->name));

					dirent->type = DIRENT_TYPE_DIRECTORY;
					dirent->fileno = 1;
				} else {
					dirent->type = DIRENT_TYPE_END;
				}

				out_count = sizeof(Dirent);

				return READ_OK;
			}

			return _complete_read_of_file_systems(dir_vfs_handle, dst, count, out_count);
		}

		Ftruncate_result ftruncate(Vfs_handle *, file_size) override
		{
			return FTRUNCATE_ERR_NO_PERM;
		}

		bool read_ready(Vfs_handle *handle) override
		{
			if (&handle->fs() == this)
				return true;

			return handle->fs().read_ready(handle);
		}

		bool notify_read_ready(Vfs_handle *handle) override
		{
			if (&handle->fs() == this)
				return true;

			return handle->fs().notify_read_ready(handle);
		}

		bool queue_sync(Vfs_handle *vfs_handle) override
		{
			bool result = true;

			Dir_vfs_handle *dir_vfs_handle =
				static_cast<Dir_vfs_handle*>(vfs_handle);

			auto f = [&result, dir_vfs_handle] (Dir_vfs_handle::Subdir_handle_element &e) {
				/* forward the handle context */
				e.vfs_handle.context = dir_vfs_handle->context;

				if (!e.vfs_handle.fs().queue_sync(&e.vfs_handle)) {
					result = false;
				}
			};

			dir_vfs_handle->subdir_handle_registry.for_each(f);

			return result;
		}

		Sync_result complete_sync(Vfs_handle *vfs_handle) override
		{
			Sync_result result = SYNC_OK;

			Dir_vfs_handle *dir_vfs_handle =
				static_cast<Dir_vfs_handle*>(vfs_handle);

			auto f = [&result, dir_vfs_handle] (Dir_vfs_handle::Subdir_handle_element &e) {
				Sync_result r = e.vfs_handle.fs().complete_sync(&e.vfs_handle);
				if (r != SYNC_OK)
					result = r;
			};

			dir_vfs_handle->subdir_handle_registry.for_each(f);

			return result;
		}
};

#endif /* _INCLUDE__VFS__DIR_FILE_SYSTEM_H_ */
