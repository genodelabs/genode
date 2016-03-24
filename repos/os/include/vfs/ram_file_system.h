/*
 * \brief  Embedded RAM VFS
 * \author Emery Hemingway
 * \date   2015-07-21
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__VFS__RAM_FILE_SYSTEM_H_
#define _INCLUDE__VFS__RAM_FILE_SYSTEM_H_

#include <ram_fs/chunk.h>
#include <vfs/file_system.h>
#include <dataspace/client.h>
#include <util/avl_tree.h>

namespace Vfs_ram {

	using namespace Genode;
	using namespace Vfs;

	class Node;
	class File;
	class Symlink;
	class Directory;

	enum { MAX_NAME_LEN = 128 };

	/**
	 * Return base-name portion of null-terminated path string
	 */
	static inline char const *basename(char const *path)
	{
		char const *start = path;

		for (; *path; ++path)
			if (*path == '/')
				start = path + 1;

		return start;
	}

}

namespace Vfs { class Ram_file_system; }


class Vfs_ram::Node : public Genode::Avl_node<Node>, public Genode::Lock
{
	private:

		char _name[MAX_NAME_LEN];

		unsigned long const _inode;

		/**
		 * Generate unique inode number
		 *
		 * XXX: these inodes could clash with other VFS plugins
		 */
		static unsigned long _unique_inode()
		{
			static unsigned long inode_count;
			return ++inode_count;
		}

	public:

		Node(char const *node_name)
		: _inode(_unique_inode()) { name(node_name); }

		virtual ~Node() { }

		unsigned long inode() const { return _inode; }

		char const *name() { return _name; }
		void name(char const *name) { strncpy(_name, name, MAX_NAME_LEN); }

		virtual Vfs::file_size length() = 0;

		/************************
		 ** Avl node interface **
		 ************************/

		bool higher(Node *c) { return (strcmp(c->_name, _name) > 0); }

		/**
		 * Find index N by walking down the tree N times,
		 * not the most efficient way to do this.
		 */
		Node *index(file_offset &i)
		{
			if (i-- == 0)
				return this;

			Node *n;

			n = child(LEFT);
			if (n)
				n = n->index(i);

			if (n) return n;

			n = child(RIGHT);
			if (n)
				n = n->index(i);

			return n;
		}

		Node *sibling(const char *name)
		{
			if (strcmp(name, _name) == 0) return this;

			Node *c =
				Avl_node<Node>::child(strcmp(name, _name) > 0);
			return c ? c->sibling(name) : 0;
		}

		struct Guard
		{
			Node *node;

			Guard(Node *guard_node) : node(guard_node) { node->lock(); }

			~Guard() { node->unlock(); }
		};

};


class Vfs_ram::File : public Vfs_ram::Node
{
	private:

		typedef ::File_system::Chunk<4096>      Chunk_level_3;
		typedef ::File_system::Chunk_index<128, Chunk_level_3> Chunk_level_2;
		typedef ::File_system::Chunk_index<64,  Chunk_level_2> Chunk_level_1;
		typedef ::File_system::Chunk_index<64,  Chunk_level_1> Chunk_level_0;

		Chunk_level_0 _chunk;

		file_size _length = 0;

	public:

		File(char const *name, Allocator &alloc)
		: Node(name), _chunk(alloc, 0) { }

		size_t read(char *dst, size_t len, file_size seek_offset)
		{
			file_size const chunk_used_size = _chunk.used_size();

			if (seek_offset >= _length)
				return 0;

			/*
			 * Constrain read transaction to available chunk data
			 *
			 * Note that 'chunk_used_size' may be lower than '_length'
			 * because 'Chunk' may have truncated tailing zeros.
			 */
			if (seek_offset + len >= _length)
				len = _length - seek_offset;

			file_size read_len = len;

			if (seek_offset + read_len > chunk_used_size) {
				if (chunk_used_size >= seek_offset)
					read_len = chunk_used_size - seek_offset;
				else
					read_len = 0;
			}

			_chunk.read(dst, read_len, seek_offset);

			/* add zero padding if needed */
			if (read_len < len)
				memset(dst + read_len, 0, len - read_len);

			return len;
		}

		size_t write(char const *src, size_t len, file_size seek_offset)
		{
			if (seek_offset == (file_size)(~0))
				seek_offset = _chunk.used_size();

			if (seek_offset + len >= Chunk_level_0::SIZE)
				len = Chunk_level_0::SIZE - (seek_offset + len);

			_chunk.write(src, len, (size_t)seek_offset);

			/*
			 * Keep track of file length. We cannot use 'chunk.used_size()'
			 * as file length because trailing zeros may by represented
			 * by zero chunks, which do not contribute to 'used_size()'.
			 */
			_length = max(_length, seek_offset + len);

			return len;
		}

		file_size length() { return _length; }

		void truncate(file_size size)
		{
			if (size < _chunk.used_size())
				_chunk.truncate(size);

			_length = size;
		}
};


class Vfs_ram::Symlink : public Vfs_ram::Node
{
	private:

		char   _target[MAX_PATH_LEN];
		size_t _len = 0;

	public:

		Symlink(char const *name) : Node(name) { }

		file_size length() { return _len; }

		void set(char const *target, size_t len)
		{
			_len = min(len, MAX_PATH_LEN);
			memcpy(_target, target, _len);
		}

		size_t get(char *buf, size_t len)
		{
			size_t out = min(len, _len);
			memcpy(buf, _target, out);
			return out;
		}
};


class Vfs_ram::Directory : public Vfs_ram::Node
{
	private:

		Avl_tree<Node> _entries;
		file_size _count = 0;

	public:

		Directory(char const *name) : Node(name) { }

		~Directory()
		{
			while (Node *node = _entries.first()) {
				_entries.remove(node);
				destroy(env()->heap(), node);
			}
		}

		void adopt(Node *node)
		{
			_entries.insert(node);
			++_count;
		}

		Node *child(char const *name)
		{
			Node *node = _entries.first();
			return node ? node->sibling(name) : 0;
		}

		void release(Node *node)
		{
			_entries.remove(node);
			--_count;
		}

		file_size length() override { return _count; }

		void dirent(file_offset index, Directory_service::Dirent &dirent)
		{
			dirent.fileno = index+1;
			dirent.type = Directory_service::DIRENT_TYPE_END;
			dirent.name[0] = '\0';

			Node *node = _entries.first();
			if (node) {
				node = node->index(index);
				if (!node) return;

				strncpy(dirent.name, node->name(), sizeof(dirent.name));

				File *file = dynamic_cast<File *>(node);
				if (file) {
					dirent.type = Directory_service::DIRENT_TYPE_FILE;
					return;
				}

				Directory *dir = dynamic_cast<Directory *>(node);
				if (dir) {
					dirent.type = Directory_service::DIRENT_TYPE_DIRECTORY;
					return;
				}

				Symlink *symlink = dynamic_cast<Symlink *>(node);
				if (symlink) {
					dirent.type = Directory_service::DIRENT_TYPE_SYMLINK;
				}
			}
		}
};


class Vfs::Ram_file_system : public Vfs::File_system
{
	private:

		class Ram_vfs_handle : public Vfs_handle
		{
			private:

				Vfs_ram::File *_file;

			public:
 
				Ram_vfs_handle(File_system &fs, int status_flags, Vfs_ram::File *file)
				: Vfs_handle(fs, fs, status_flags), _file(file)
				{ }
 
				Vfs_ram::File *file() const { return _file; }
		};

		Genode::Allocator  &_alloc;
		Vfs_ram::Directory  _root;

		Vfs_ram::Node *lookup(char const *path, bool return_parent = false)
		{
			using namespace Vfs_ram;

			if (*path ==  '/') ++path;
			if (*path == '\0') return &_root;

			char buf[Vfs::MAX_PATH_LEN];
			strncpy(buf, path, Vfs::MAX_PATH_LEN);
			Directory *dir = &_root;

			char *name = &buf[0];
			for (size_t i = 0; i < MAX_PATH_LEN; ++i) {
				if (buf[i] == '/') {
					buf[i] = '\0';

					Node *node = dir->child(name);
					if (!node) return 0;

					dir = dynamic_cast<Directory *>(node);
					if (!dir) return 0;

					/* set the current name aside */
					name = &buf[i+1];
				} else if (buf[i] == '\0') {
					if (return_parent)
						return dir;
					else
						return dir->child(name);
				}
			}
			return 0;
		}

		Vfs_ram::Directory *lookup_parent(char const *path)
		{
			using namespace Vfs_ram;

			Node *node = lookup(path, true);
			if (node)
				return dynamic_cast<Directory *>(node);
			return 0;
		}

	public:

		Ram_file_system(Xml_node config)
		: _alloc(*env()->heap()), _root("") { }

		/*********************************
		 ** Directory service interface **
		 *********************************/

		file_size num_dirent(char const *path) override
		{
			using namespace Vfs_ram;

			if (Node *node = lookup(path)) {
				Node::Guard guard(node);
				if (Directory *dir = dynamic_cast<Directory *>(node))
					return dir->length();
			}

			return 0;
		}

		bool is_directory(char const *path)
		{
			using namespace Vfs_ram;

			Node *node = lookup(path);
			return node ? dynamic_cast<Directory *>(node) : 0;
		}

		char const *leaf_path(char const *path) {
			return lookup(path) ? path : 0; }

		Mkdir_result mkdir(char const *path, unsigned mode) override
		{
			using namespace Vfs_ram;

			Directory *parent = lookup_parent(path);
			if (!parent) return MKDIR_ERR_NO_ENTRY;
			Node::Guard guard(parent);

			char const *name = basename(path);

			if (strlen(name) >= MAX_NAME_LEN)
				return MKDIR_ERR_NAME_TOO_LONG;

			if (parent->child(name)) return MKDIR_ERR_EXISTS;

			parent->adopt(new (_alloc) Directory(name));
			return MKDIR_OK;
		}

		Open_result open(char const *path, unsigned mode, Vfs_handle **handle) override
		{
			using namespace Vfs_ram;

			File *file;
			char const *name = basename(path);

			if (mode & OPEN_MODE_CREATE) {
				Directory *parent = lookup_parent(path);
				Node::Guard guard(parent);

				if (!parent) return OPEN_ERR_UNACCESSIBLE;
				if (parent->child(name)) return OPEN_ERR_EXISTS;

				if (strlen(name) >= MAX_NAME_LEN)
					return OPEN_ERR_NAME_TOO_LONG;

				file = new (_alloc) File(name, _alloc);
				parent->adopt(file);
			} else {
				Node *node = lookup(path);
				if (!node) return OPEN_ERR_UNACCESSIBLE;

				file = dynamic_cast<File *>(node);
				if (!file) return OPEN_ERR_UNACCESSIBLE;
			}

			/* allocate handle on the heap */
			*handle = new (env()->heap()) Ram_vfs_handle(*this, mode, file);
			return OPEN_OK;
		}

		Stat_result stat(char const *path, Stat &stat) override
		{
			using namespace Vfs_ram;

			Node *node = lookup(path);
			if (!node) return STAT_ERR_NO_ENTRY;
			Node::Guard guard(node);

			stat.inode = node->inode();
			stat.size  = node->length();

			File *file = dynamic_cast<File *>(node);
			if (file) {
				stat.mode = STAT_MODE_FILE | 0777;
				return STAT_OK;
			}

			Directory *dir = dynamic_cast<Directory *>(node);
			if (dir) {
				stat.mode = STAT_MODE_DIRECTORY | 0777;
				return STAT_OK;
			}

			Symlink *symlink = dynamic_cast<Symlink *>(node);
			if (symlink) {
				stat.mode = STAT_MODE_SYMLINK | 0777;
				return STAT_OK;
			}

			/* this should never happen */
			return STAT_ERR_NO_ENTRY;
		}

		Dirent_result dirent(char const *path, file_offset index, Dirent &dirent) override
		{
			using namespace Vfs_ram;

			Node *node = lookup(path);
			if (!node) return DIRENT_ERR_INVALID_PATH;
			Node::Guard guard(node);

			Directory *dir = dynamic_cast<Directory *>(node);
			if (!dir) return DIRENT_ERR_INVALID_PATH;

			dir->dirent(index, dirent);
			return DIRENT_OK;
		}

		Symlink_result symlink(char const *target, char const *path) override
		{
			using namespace Vfs_ram;

			Symlink *link;
			Directory *parent = lookup_parent(path);
			if (!parent) return SYMLINK_ERR_NO_ENTRY;
			Node::Guard guard(parent);

			char const *name = basename(path);

			Node *node = parent->child(name);
			if (node) {
				node->lock();
				link = dynamic_cast<Symlink *>(node);
				if (!link) {
					node->unlock();
					return SYMLINK_ERR_EXISTS;
				}
			} else {
				if (strlen(name) >= MAX_NAME_LEN)
					return SYMLINK_ERR_NAME_TOO_LONG;

				link = new (_alloc) Symlink(name);
				link->lock();
				parent->adopt(link);
			}

			if (*target)
				link->set(target, strlen(target));
			link->unlock();
			return SYMLINK_OK;
		}

		Readlink_result readlink(char const *path, char *buf,
		                         file_size buf_size, file_size &out_len) override
		{
			using namespace Vfs_ram;

			Directory *parent = lookup_parent(path);
			if (!parent)
				return READLINK_ERR_NO_ENTRY;
			Node::Guard parent_guard(parent);

			Node *node = parent->child(basename(path));
			if (!node) return READLINK_ERR_NO_ENTRY;
			Node::Guard guard(node);

			Symlink *link = dynamic_cast<Symlink *>(node);
			if (!link) return READLINK_ERR_NO_ENTRY;

			out_len = link->get(buf, buf_size);
			return READLINK_OK;
		}

		Rename_result rename(char const *from, char const *to) override
		{
			using namespace Vfs_ram;

			if ((strcmp(from, to) == 0) && lookup(from))
				return RENAME_OK;

			char const *new_name = basename(to);
			if (strlen(new_name) >= MAX_NAME_LEN)
				return RENAME_ERR_NO_PERM;

			Directory *from_dir = lookup_parent(from);
			if (!from_dir) return RENAME_ERR_NO_ENTRY;
			Node::Guard from_guard(from_dir);

			Directory *to_dir = lookup_parent(to);
			if (!to_dir) return RENAME_ERR_NO_ENTRY;

			/* unlock the node so a second guard can be constructed */
			if (from_dir == to_dir)
				from_dir->unlock();

			Node::Guard to_guard(to_dir);

			Node *from_node = from_dir->child(basename(from));
			if (!from_node) return RENAME_ERR_NO_ENTRY;
			Node::Guard guard(from_node);

			Node *to_node = to_dir->child(new_name);
			if (to_node) {
				to_node->lock();

				if (Directory *dir = dynamic_cast<Directory*>(to_node))
					if (dir->length() || (!dynamic_cast<Directory*>(from_node)))
						return RENAME_ERR_NO_PERM;

				to_dir->release(to_node);
				destroy(_alloc, to_node);
			}

			from_dir->release(from_node);
			from_node->name(new_name);
			to_dir->adopt(from_node);

			return RENAME_OK;
		}

		Unlink_result unlink(char const *path) override
		{
			using namespace Vfs_ram;

			Directory *parent = lookup_parent(path);
			if (!parent) return UNLINK_ERR_NO_ENTRY;
			Node::Guard guard(parent);

			Node *node = parent->child(basename(path));
			if (!node) return UNLINK_ERR_NO_ENTRY;

			node->lock();
			parent->release(node);
			destroy(_alloc, node);
			return UNLINK_OK;
		}

		Dataspace_capability dataspace(char const *path) override
		{
			using namespace Vfs_ram;

			Ram_dataspace_capability ds_cap;

			Node *node = lookup(path);
			if (!node) return ds_cap;
			Node::Guard guard(node);

			File *file = dynamic_cast<File *>(node);
			if (!file) return ds_cap;

			size_t len = file->length();

			char *local_addr = nullptr;
			try {
				ds_cap = env()->ram_session()->alloc(len);

				local_addr = env()->rm_session()->attach(ds_cap);
				file->read(local_addr, file->length(), 0);
				env()->rm_session()->detach(local_addr);

			} catch(...) {
				env()->rm_session()->detach(local_addr);
				env()->ram_session()->free(ds_cap);
				return Dataspace_capability();
			}
			return ds_cap;
		}

		void release(char const *path, Dataspace_capability ds_cap) override {
			env()->ram_session()->free(
				static_cap_cast<Genode::Ram_dataspace>(ds_cap)); }


		/************************
		 ** File I/O interface **
		 ************************/

		Write_result write(Vfs_handle *vfs_handle,
		                   char const *buf, file_size len,
		                   Vfs::file_size &out) override
		{
			if (!(vfs_handle->status_flags() & (OPEN_MODE_WRONLY|OPEN_MODE_RDWR)))
				return WRITE_ERR_INVALID;

			Ram_vfs_handle const *handle =
				static_cast<Ram_vfs_handle *>(vfs_handle);

			Vfs_ram::Node::Guard guard(handle->file());
			out = handle->file()->write(buf, len, handle->seek());

			return WRITE_OK;
		}

		Read_result read(Vfs_handle *vfs_handle,
		                 char *buf, file_size len,
		                 file_size &out) override
		{
			if ((vfs_handle->status_flags() & OPEN_MODE_ACCMODE) == OPEN_MODE_WRONLY)
				return READ_ERR_INVALID;

			Ram_vfs_handle const *handle =
				static_cast<Ram_vfs_handle *>(vfs_handle);

			Vfs_ram::Node::Guard guard(handle->file());

			out = handle->file()->read(buf, len, handle->seek());
			return READ_OK;
		}

		Ftruncate_result ftruncate(Vfs_handle *vfs_handle, file_size len) override
		{
			if (!(vfs_handle->status_flags() & (OPEN_MODE_WRONLY|OPEN_MODE_RDWR)))
				return FTRUNCATE_ERR_NO_PERM;

			Ram_vfs_handle const *handle =
				static_cast<Ram_vfs_handle *>(vfs_handle);

			Vfs_ram::Node::Guard guard(handle->file());
			handle->file()->truncate(len);
			return FTRUNCATE_OK;
		}


		/***************************
		 ** File_system interface **
		 ***************************/

		static char const *name() { return "ram"; }
};

#endif /* _INCLUDE__VFS__RAM_FILE_SYSTEM_H_ */
