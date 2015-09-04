/*
 * \brief  VFS server node cache
 * \author Emery Hemingway
 * \date   2015-09-02
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _VFS__NODE_CACHE_H_
#define _VFS__NODE_CACHE_H_

/* Genode includes */
#include <file_system/node.h>
#include <vfs/file_system.h>
#include <util/avl_string.h>
#include <util/noncopyable.h>

namespace File_system {

	struct Node;
	struct Directory;
	class  File;
	struct Symlink;
	class Node_cache;

	typedef Avl_string<MAX_PATH_LEN> Avl_path_string;

	Vfs::File_system *root();
}


/**
 * Reference counted node object that can be inserted
 * into an AVL tree.
 */
class File_system::Node : public Node_base, public Avl_node<Node>, private Noncopyable
{
	friend class Node_cache;

	private:

		unsigned _ref_count;
		char     _path[Vfs::MAX_PATH_LEN];

	protected:

		void incr() { ++_ref_count; }
		void decr() { --_ref_count; }

		bool in_use() const { return _ref_count; }

	public:

		Node(char const *path)
		: _ref_count(0) {
			strncpy(_path, path, sizeof(_path)); }

		char const *path() const { return _path; }
		void path(char const *new_path) {
			strncpy(_path, new_path, sizeof(_path)); }

		virtual size_t read(char *dst, size_t len, seek_off_t) { return 0; }
		virtual size_t write(char const *src, size_t len, seek_off_t) { return 0; }

		/************************
		 ** Avl node interface **
		 ************************/

		bool higher(Node *n) { return (strcmp(n->_path, _path) > 0); }

		/**
		 * Find by path
		 */
		Node *find_by_path(const char *path)
		{
			if (strcmp(path, _path) == 0) return this;

			Node *n = Avl_node<Node>::child(strcmp(path, _path) > 0);
			return n ? n->find_by_path(path) : nullptr;
		}
};


struct File_system::Directory : Node
{
	Directory(char const *path, bool create)
	: Node(path)
	{
		if (create)
			assert_mkdir(root()->mkdir(path, 0777));
		else if (strcmp("/", path, 2) == 0)
			return;
		else if (!root()->leaf_path(path))
			throw Lookup_failed();
		else if (!root()->is_directory(path))
			throw Node_already_exists();
	}

	size_t read(char *dst, size_t len, seek_off_t seek_offset)
	{
		Directory_service::Dirent vfs_dirent;
		size_t blocksize = sizeof(File_system::Directory_entry);

		int index = (seek_offset / blocksize);

		size_t remains = len;

		while (remains >= blocksize) {
			memset(&vfs_dirent, 0x00, sizeof(vfs_dirent));
			if (root()->dirent(path(), index++, vfs_dirent)
				!= Vfs::Directory_service::DIRENT_OK)
				return len - remains;

			File_system::Directory_entry *fs_dirent = (Directory_entry *)dst;
			switch (vfs_dirent.type) {
			case Vfs::Directory_service::DIRENT_TYPE_DIRECTORY:
				fs_dirent->type = File_system::Directory_entry::TYPE_DIRECTORY;
				break;
			case Vfs::Directory_service::DIRENT_TYPE_SYMLINK:
				fs_dirent->type = File_system::Directory_entry::TYPE_SYMLINK;
				break;
			case Vfs::Directory_service::DIRENT_TYPE_FILE:
			default:
				fs_dirent->type = File_system::Directory_entry::TYPE_FILE;
				break;
			}
			strncpy(fs_dirent->name, vfs_dirent.name, MAX_NAME_LEN);

			remains -= blocksize;
			dst += blocksize;
		}
		return len - remains;
	}
};


class File_system::File : public Node
{
	private:

		Vfs_handle *_handle;
		unsigned    _mode;

	public:

		File(char const *path, Mode fs_mode, bool create)
		:
			Node(path),
			_handle(nullptr)
		{
			switch (fs_mode) {
			case STAT_ONLY:
			case READ_ONLY:
				_mode = Directory_service::OPEN_MODE_RDONLY; break;
			case WRITE_ONLY:
			case READ_WRITE:
				_mode = Directory_service::OPEN_MODE_RDWR;   break;
			}

			unsigned mode = create ?
				_mode | Directory_service::OPEN_MODE_CREATE : _mode;

			assert_open(root()->open(path, mode, &_handle));
		}

		~File() { destroy(env()->heap(), _handle); }

		void open(Mode fs_mode)
		{
			if (_mode & Directory_service::OPEN_MODE_RDWR) return;

			unsigned mode;
			switch (fs_mode) {
			case WRITE_ONLY:
			case READ_WRITE:
				mode = Directory_service::OPEN_MODE_RDWR;   break;
			default:
				return;
			}
			if (mode == _mode) return;

			Vfs_handle *new_handle = nullptr;
			assert_open(root()->open(path(), mode, &new_handle));
			destroy(env()->heap(), _handle);
			_handle = new_handle;
			_mode   = mode;
		}

		void truncate(file_size_t size) {
			assert_truncate(_handle->fs().ftruncate(_handle, size)); }

		size_t read(char *dst, size_t len, seek_off_t seek_offset)
		{
			Vfs::file_size res = 0;
			_handle->seek(seek_offset);
			_handle->fs().read(_handle, dst, len, res);
			return res;
		}

		size_t write(char const *src, size_t len, seek_off_t seek_offset)
		{
			Vfs::file_size res = 0;
			_handle->seek(seek_offset);
			_handle->fs().write(_handle, src, len, res);
			mark_as_updated();
			return res;
		}
};


struct File_system::Symlink : Node
{
	Symlink(char const *path, bool create)
	: Node(path)
	{
		if (create)
			assert_symlink(root()->symlink("", path));
		else if (!root()->leaf_path(path))
			throw Lookup_failed();
		else {
			Vfs::Directory_service::Stat s;
			assert_stat(root()->stat(path, s));
			if (!(s.mode & Vfs::Directory_service::STAT_MODE_SYMLINK))
				throw Node_already_exists();
		}
	}

	size_t read(char *dst, size_t len, seek_off_t seek_offset)
	{
		Vfs::file_size res = 0;
		root()->readlink(path(), dst, len, res);
		return res;
	}

	size_t write(char const *src, size_t len, seek_off_t seek_offset)
	{
		root()->unlink(path());
		size_t n = (root()->symlink(src, path()) == Directory_service::SYMLINK_OK)
			? len : 0;

		if (n) {
			mark_as_updated();
			notify_listeners();
		}
		return n;
	}
};


/**
 * This structure deduplicates nodes between sessions, without it
 * the signal notifications would not propagate between sessions.
 */
struct File_system::Node_cache : Avl_tree<Node>
{
	Node *find(char const *path) {
		return first() ? (Node *)first()->find_by_path(path) : nullptr; }

	void free(Node *node)
	{
		node->decr();
		if (node->in_use())
			return;

		remove(node);
		destroy(env()->heap(), node);
	}

	void remove_path(char const *path)
	{
		Node *node = find(path);
		if (!node ) return;

		remove(node);
		destroy(env()->heap(), node);
	}

	void rename(char const *from, char const *to)
	{
		Node *node = find(to);
		if (node)
			throw Permission_denied();

		node = find(from);
		if (!node ) return;

		remove(node);
		node->path(to);
		insert(node);
		node->mark_as_updated();
	}

	/**
	 * Return an existing node or query the VFS
	 * and alloc a proper node.
	 */
	Node *node(char const *path)
	{
		Node *node = find(path);
		if (!node) {
			Directory_service::Stat stat;
			assert_stat(root()->stat(path, stat));

			switch (stat.mode & (
				Directory_service::STAT_MODE_DIRECTORY |
				Directory_service::STAT_MODE_SYMLINK |
				File_system::Status::MODE_FILE)) {

			case Directory_service::STAT_MODE_DIRECTORY:
				node = new (env()->heap()) Directory(path, false);
				break;

			case Directory_service::STAT_MODE_SYMLINK:
				node = new (env()->heap()) Symlink(path, false);
				break;

			default: /* Directory_service::STAT_MODE_FILE */
				node = new (env()->heap()) File(path, READ_ONLY, false);
				break;
			}
			insert(node);
		}
		node->incr();
		return node;
	}

	Directory *directory(char const *path, bool create)
	{
		Directory *dir;
		Node *node = find(path);
		if (node) {
			dir = dynamic_cast<Directory *>(node);
			if (!dir)
				throw Node_already_exists();

		} else {
			dir = new (env()->heap()) Directory(path, create);
			insert(dir);
		}
		dir->incr();
		return dir;
	}

	File *file(char const *path, Mode mode, bool create)
	{
		File *file;
		Node *node = find(path);
		if (node) {
			file = dynamic_cast<File *>(node);
			if (!file)
				throw Node_already_exists();

			file->open(mode);

		} else {
			file = new (env()->heap()) File(path, mode, create);
			insert(file);
		}
		file->incr();
		return file;
	}

	Symlink *symlink(char const *path, bool create)
	{
		Symlink *link;
		Node *node = find(path);
		if (node) {
			link = dynamic_cast<Symlink *>(node);
			if (!link)
				throw Node_already_exists();

		} else {
			link = new (env()->heap()) Symlink(path, create);
			insert(link);
		}
		link->incr();
		return link;
	}
};

#endif
