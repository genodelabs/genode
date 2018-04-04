/*
 * \brief  Copy-on-write file-system
 * \author Emery Hemingway
 * \date   2018-03-22
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__VFS__COW_FILE_SYSTEM_H_
#define _SRC__VFS__COW_FILE_SYSTEM_H_

#include <vfs/env.h>
#include <vfs/file_system_factory.h>
#include <vfs/vfs_handle.h>
#include <vfs/types.h>


namespace Vfs_cow {
	using namespace Vfs;
	using namespace Genode;
	class File_system;
}


class Vfs_cow::File_system : public Vfs::File_system
{
	private:

		static Absolute_path _config_path(Genode::Xml_node &node, char const *key)
		{
			Genode::String<Absolute_path::capacity()> str{};
			node.attribute(key).value(&str);
			return Absolute_path(str.string());
		}

		Genode::Allocator  &_alloc;
		Vfs::File_system   &_root_dir;
		Genode::Entrypoint &_ep;

		Absolute_path const _ro_root_path;
		Absolute_path const _rw_root_path;

		inline Absolute_path _ro_path(char const *path) const {
			return Absolute_path(path+1, _ro_root_path.string()); }

		inline Absolute_path _rw_path(char const *path) const {
			return Absolute_path(path+1, _rw_root_path.string()); }

		inline bool _leaf(Absolute_path const &path) {
			return _root_dir.leaf_path(path.string()) != nullptr; }

		inline bool _ro_leaf(char const *path) {
			return _leaf(_ro_path(path)); }

		inline bool _rw_leaf(char const *path) {
			return _leaf(_ro_path(path)); }

		void _mkdirs(Absolute_path const &path)
		{
			Vfs_handle *dir_handle = nullptr;
			Opendir_result res = _root_dir.opendir(
				path.string(), true, &dir_handle, _alloc);
			if (res == OPENDIR_ERR_LOOKUP_FAILED) {
				Absolute_path parent(path);
				parent.strip_last_element();
				_mkdirs(parent);
				_root_dir.opendir(path.string(), true, &dir_handle, _alloc);
			}
			if (dir_handle) dir_handle->ds().close(dir_handle);
		}

		bool _copy(Absolute_path const &from, Absolute_path const &to)
		{
			Vfs_handle *roh = nullptr;
			Vfs_handle *rwh = nullptr;

			_root_dir.open(
				from.string(), OPEN_MODE_RDONLY, &roh, _alloc);
			if (!roh)
				return false;

			_root_dir.open(
				to.string(), OPEN_MODE_WRONLY|OPEN_MODE_CREATE, &rwh, _alloc);

			if (!rwh) {
				roh->ds().close(roh);
				return false;
			}

			if (!roh || !rwh)
			{
				return false;
			}

			char buf[1<<14];
			Stat sb { };
			_root_dir.stat(from.string(), sb);
			file_size remain = sb.size;

			while (remain > 0) {
				file_size rn = 0;
				file_size wn = 0;

				while (!roh->fs().queue_read(roh, sizeof(buf))) {
					warning("COW: blocking for replication...");
					_ep.wait_and_dispatch_one_io_signal();
				}

				Read_result rres  = roh->fs().complete_read(
					roh, buf, sizeof(buf), rn);
				switch (rres) {
				case READ_OK: break;
				case READ_QUEUED: continue;
				default: remain = -1; continue;
				}

				Write_result wres = rwh->fs().write(rwh, buf, rn, wn);
				switch (wres) {
				case WRITE_OK: break;
				case WRITE_ERR_AGAIN:
				case WRITE_ERR_WOULD_BLOCK:
					continue;
				default:
					_root_dir.unlink(from.string());
					remain = -1; continue;
				}

				roh->advance_seek(wn);
				rwh->advance_seek(wn);
				remain -= wn;
			}

			roh->ds().close(roh);
			rwh->ds().close(rwh);
			bool res = (remain == 0);
			if (res)
				log("COW: replicated from ", from, " to ", to);
			else
				error("COW: replication from ", from, " to ", to, " failed");
			return res;
		}

		struct Cow_dir_handle : Vfs::Vfs_handle
		{
			Vfs_handle &ro;
			Vfs_handle &rw;

			Absolute_path const rw_leaf;

			Cow_dir_handle(Vfs::File_system &fs,
			               Genode::Allocator &alloc,
			               Vfs_handle &roh,
			               Vfs_handle &rwh,
			               char const *rw_leaf)
			: Vfs_handle(fs, fs, alloc, 0), ro(roh), rw(rwh), rw_leaf(rw_leaf) { }

			~Cow_dir_handle()
			{
				ro.ds().close(&ro);
				rw.ds().close(&rw);
			}

			/**
			 * Apply an operation to the RW or RO handle
			 * depending on current seek position.
			 */
			template <typename FN>
			void apply_seek(FN const &fn)
			{
				file_size const index = seek() / sizeof(Dirent);
				file_size const rw_dirents = rw.ds().num_dirent(rw_leaf.string());

				/* read from RW directory first */
				if (index < rw_dirents) {
					rw.seek(index);
					fn(rw);
				} else {
					ro.seek(index - rw_dirents);
					fn(ro);
				}
			}
		};

	public:

		File_system(Vfs::Env &vfs_env, Genode::Xml_node config)
		:
			_alloc(vfs_env.alloc()),
			_root_dir(vfs_env.root_dir()),
			_ep(vfs_env.env().ep()),
			_ro_root_path(_config_path(config, "ro")),
			_rw_root_path(_config_path(config, "rw"))
		{ }

		const char* type() override { return "cow"; }

		static const char* name() { return "cow"; }

		/***********************
		 ** Directory service **
		 ***********************/

		Genode::Dataspace_capability dataspace(const char *path) override
		{
			auto rw_path = _rw_path(path);
			if (_leaf(rw_path)) {
				return _root_dir.dataspace(rw_path.string());
			} else {
				auto ro_path = _ro_path(path);
				return _root_dir.dataspace(ro_path.string());
			}
		}

		void release(char const *path, Dataspace_capability ds) override
		{
			_root_dir.release(_ro_path(path).string(), ds);
			_root_dir.release(_rw_path(path).string(), ds);
		}

		Open_result open(const char *path,
		                 unsigned int mode,
		                 Vfs::Vfs_handle **out,
		                 Genode::Allocator &alloc) override
		{
			auto ro_path = _ro_path(path);
			auto rw_path = _rw_path(path);

			if (mode & OPEN_MODE_CREATE) {
				if (_leaf(ro_path)) {
					return OPEN_ERR_EXISTS;
				} else {
					return _root_dir.open(
						rw_path.string(), mode, out, alloc);
				}
			}

			Open_result rw_res = _root_dir.open(
				rw_path.string(), mode, out, alloc);

			if (rw_res == OPEN_ERR_UNACCESSIBLE) {
				_copy(ro_path, rw_path);
				rw_res = _root_dir.open(
					rw_path.string(), mode, out, alloc);
			}

			return rw_res;
		}

		Opendir_result opendir(char const *path, bool create,
	                           Vfs_handle **out, Allocator &alloc) override
		{
			auto ro_path = _ro_path(path);
			auto rw_path = _rw_path(path);
			Opendir_result res = OPENDIR_ERR_PERMISSION_DENIED;

			if (!_leaf(ro_path)) {
				return _root_dir.opendir(
					rw_path.string(), create, out, alloc);
			}

			if (create)
				return OPENDIR_ERR_NODE_ALREADY_EXISTS;

			{
				Vfs_handle *roh = nullptr;
				Vfs_handle *rwh = nullptr;

				res = _root_dir.opendir(
					ro_path.string(), false, &roh, alloc);

				if (res != OPENDIR_OK)
					return res;

				char const *rw_leaf = _root_dir.leaf_path(rw_path.string());
				if (!rw_leaf) {
					_mkdirs(rw_path);
					rw_leaf = _root_dir.leaf_path(rw_path.string());
				}

				res = _root_dir.opendir(
					rw_path.string(), false, &rwh, alloc);

				if (res != OPENDIR_OK) {
					roh->ds().close(roh);
					return res;
				}

				*out = new (alloc)
					Cow_dir_handle(*this, alloc, *roh, *rwh, rw_leaf);
				return OPENDIR_OK;
			}
		}

		void close(Vfs::Vfs_handle *vfs_handle) override
		{
			if (&vfs_handle->ds() == this) {
				Cow_dir_handle *h = static_cast<Cow_dir_handle*>(vfs_handle);
				destroy(h->alloc(), h);
			} else {
				Genode::error("unknown handle");
			}
		}

		Watch_result watch(char const *path,
		                   Vfs_watch_handle **out,
		                   Allocator &alloc) override
		{
			auto const rw_path = _rw_path(path);
			if (!_leaf(rw_path)) {
				if (_root_dir.directory(_ro_path(path).string())) {
					_mkdirs(rw_path);
				} else {
					return WATCH_ERR_UNACCESSIBLE;
				}
			}
			return _root_dir.watch(rw_path.string(), out, alloc);
		}

		Stat_result stat(const char *path, Vfs::Directory_service::Stat &buf) override
		{
			Stat_result res = _root_dir.stat(_rw_path(path).string(), buf);
			if (res != STAT_OK)
				res = _root_dir.stat(_ro_path(path).string(), buf);
			return res;
		}

		Unlink_result unlink(const char *path) override
		{
			if (_ro_leaf(path))
				return UNLINK_ERR_NO_PERM;
			return _root_dir.unlink(_rw_path(path).string());
		}

		Rename_result rename(const char *from , const char *to) override
		{
			return _root_dir.rename(
				_rw_path(from).string(), _rw_path(to).string());
		}

		file_size num_dirent(const char *path) override
		{
			/* return a simple sum */
			return
				_root_dir.num_dirent(_rw_path(path).string())+
				_root_dir.num_dirent(_ro_path(path).string());
		}

		bool directory(char const *path) override
		{
			return _root_dir.directory(_ro_path(path).string()) ?
				true : _root_dir.directory(_rw_path(path).string());
		}

		const char* leaf_path(const char *path) override
		{
			char const *res = _root_dir.leaf_path(_ro_path(path).string());
			if (res == nullptr)
				res = _root_dir.leaf_path(_rw_path(path).string());
			return res;
		}

		/**********************
		 ** File I/O service **
		 **********************/

		Write_result write(Vfs_handle*,
		                   const char*, file_size,
		                   file_size&) override
		{
			return WRITE_ERR_INVALID;
		}

		bool queue_read(Vfs_handle *vfs_handle, file_size len) override
		{
			bool res = true;
			Cow_dir_handle *handle =
				dynamic_cast<Cow_dir_handle*>(vfs_handle);
			if (handle)
				handle->apply_seek([&] (Vfs_handle &dir) {
					res = dir.fs().queue_read(&dir, len);
				});
			return res;
		}

		Read_result complete_read(Vfs_handle *vfs_handle,
		                          char *buf, file_size len,
		                          file_size &out) override
		{
			Read_result res = READ_ERR_INVALID;
			Cow_dir_handle *handle =
				dynamic_cast<Cow_dir_handle*>(vfs_handle);
			if (handle)
				handle->apply_seek([&] (Vfs_handle &dir) {
					res = dir.fs().complete_read(&dir, buf, len, out);
				});
			return res;
		}

		bool read_ready(Vfs_handle *vfs_handle) override
		{
			bool res = true;
			Cow_dir_handle *handle =
				dynamic_cast<Cow_dir_handle*>(vfs_handle);
			if (handle)
				handle->apply_seek([&] (Vfs_handle &dir) {
					res = dir.fs().read_ready(&dir);
				});
			return res;
		}

		bool notify_read_ready(Vfs_handle *vfs_handle) override
		{
			bool res = true;
			Cow_dir_handle *handle =
				dynamic_cast<Cow_dir_handle*>(vfs_handle);
			if (handle)
				handle->apply_seek([&] (Vfs_handle &dir) {
					res = dir.fs().notify_read_ready(&dir);
				});
			return res;
		}

		Ftruncate_result ftruncate(Vfs_handle*, file_size) override {
			return FTRUNCATE_ERR_NO_PERM; }

		bool check_unblock(Vfs_handle *vfs_handle, bool rd, bool wr, bool ex) override
		{
			bool res = true;
			Cow_dir_handle *handle =
				dynamic_cast<Cow_dir_handle*>(vfs_handle);
			if (handle)
				handle->apply_seek([&] (Vfs_handle &dir) {
					res = dir.fs().check_unblock(&dir, rd, wr, ex);
				});
			return res;
		}

		void register_read_ready_sigh(Vfs_handle *vfs_handle, Signal_context_capability sigh) override
		{
			Cow_dir_handle *handle =
				dynamic_cast<Cow_dir_handle*>(vfs_handle);
			if (handle) {
				handle->rw.fs().register_read_ready_sigh(&handle->rw, sigh);
				handle->ro.fs().register_read_ready_sigh(&handle->ro, sigh);
			}
		}

		bool queue_sync(Vfs_handle *vfs_handle) override
		{
			bool res = true;
			Cow_dir_handle *handle =
				dynamic_cast<Cow_dir_handle*>(vfs_handle);
			if (handle)
				handle->apply_seek([&] (Vfs_handle &dir) {
					res = dir.fs().queue_sync(&dir);
				});
			return res;
		}

		Sync_result complete_sync(Vfs_handle *vfs_handle) override
		{
			Sync_result res = SYNC_OK;
			Cow_dir_handle *handle =
				dynamic_cast<Cow_dir_handle*>(vfs_handle);
			if (handle)
				handle->apply_seek([&] (Vfs_handle &dir) {
					res = dir.fs().complete_sync(&dir);
				});
			return res;
		}
};


/**************************
 ** VFS plugin interface **
 **************************/

extern "C" Vfs::File_system_factory *vfs_file_system_factory(void)
{
	struct Factory : Vfs::File_system_factory
	{
		Vfs::File_system *create(Vfs::Env &vfs_env,
		                         Genode::Xml_node node) override
		{
			return new (vfs_env.alloc())
				Vfs_cow::File_system(vfs_env, node);
		}
	};

	static Factory factory;
	return &factory;
}


#endif /* _SRC__VFS__COW_FILE_SYSTEM_H_ */
