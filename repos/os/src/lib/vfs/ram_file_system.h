/*
 * \brief  Embedded RAM VFS
 * \author Emery Hemingway
 * \date   2015-07-21
 */

/*
 * Copyright (C) 2015-2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__VFS__RAM_FILE_SYSTEM_H_
#define _INCLUDE__VFS__RAM_FILE_SYSTEM_H_

#include <ram_fs/chunk.h>
#include <ram_fs/param.h>
#include <vfs/file_system.h>
#include <dataspace/client.h>
#include <util/avl_tree.h>

namespace Vfs { class Ram_file_system; }

namespace Vfs_ram {

	using namespace Genode;
	using namespace Vfs;
	using namespace Ram_fs;
	using ::File_system::Chunk;
	using ::File_system::Chunk_index;

	struct Io_handle;
	struct Watch_handle;

	class Node;
	class File;
	class Symlink;
	class Directory;

	enum { MAX_NAME_LEN = 128 };

	typedef Genode::Allocator::Out_of_memory Out_of_memory;

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

	using Seek = File_system::Chunk_base::Seek;
}


struct Vfs_ram::Io_handle final : public  Vfs_handle,
                                  private Genode::List<Io_handle>::Element
{
	friend Genode::List<Io_handle>;

	Vfs_ram::Node &node;

	/* Track if this handle has modified its node */
	bool modifying = false;

	Io_handle(Vfs::File_system &fs,
	          Allocator       &alloc,
	          int              status_flags,
	          Vfs_ram::Node   &node)
	: Vfs_handle(fs, fs, alloc, status_flags), node(node)
	{ }
};


struct Vfs_ram::Watch_handle final : public  Vfs_watch_handle,
                                     private Genode::List<Watch_handle>::Element
{
	friend Genode::List<Watch_handle>;
	using Genode::List<Watch_handle>::Element::next;

	Vfs_ram::Node &node;

	Watch_handle(Vfs::File_system &fs,
	             Allocator        &alloc,
	             Node             &node)
	: Vfs_watch_handle(fs, alloc), node(node) { }
};


class Vfs_ram::Node : private Genode::Avl_node<Node>
{
	private:

		friend class Genode::Avl_node<Node>;
		friend class Genode::Avl_tree<Node>;
		friend class Genode::List<Io_handle>;
		friend class Genode::List<Io_handle>::Element;
		friend class Genode::List<Watch_handle>;
		friend class Genode::List<Watch_handle>::Element;
		friend class Watch_handle;
		friend class Directory;

		char _name[MAX_NAME_LEN];
		Genode::List<Io_handle>       _io_handles { };
		Genode::List<Watch_handle> _watch_handles { };

		/**
		 * Generate unique inode number
		 */
		static unsigned long _unique_inode()
		{
			static unsigned long inode_count;
			return ++inode_count;
		}

		Vfs::Timestamp _modification_time { Vfs::Timestamp::INVALID };

	public:

		unsigned long inode;

		Node(char const *node_name) : inode(_unique_inode())
		{
			name(node_name);
		}

		virtual ~Node() { }

		char const *name() { return _name; }
		void name(char const *name) { copy_cstring(_name, name, MAX_NAME_LEN); }

		virtual size_t length() = 0;

		void open(Io_handle &handle) { _io_handles.insert(&handle); }
		void open(Watch_handle &handle) { _watch_handles.insert(&handle); }

		bool opened() const
		{
			return _io_handles.first() != nullptr;
		}

		void close(Io_handle &handle)    {    _io_handles.remove(&handle); }
		void close(Watch_handle &handle) { _watch_handles.remove(&handle); }

		void notify()
		{
			for (Watch_handle *h = _watch_handles.first(); h; h = h->next())
				h->watch_response();
		}

		void unlink() { inode = 0; }
		bool unlinked() const { return inode == 0; }

		bool update_modification_timestamp(Vfs::Timestamp time)
		{
			_modification_time = time;
			return true;
		}

		Vfs::Timestamp modification_time() const { return _modification_time; }

		Vfs::Node_rwx rwx() const
		{
			return { .readable   = true,
			         .writeable  = true,
			         .executable = true };
		}

		virtual size_t read(Byte_range_ptr const &, Seek)
		{
			Genode::error("Vfs_ram::Node::read() called");
			return 0;
		}

		virtual Vfs::File_io_service::Read_result complete_read(Byte_range_ptr const &,
		                                                        Seek,
		                                                        size_t & /* out count */)
		{
			Genode::error("Vfs_ram::Node::complete_read() called");
			return Vfs::File_io_service::READ_ERR_INVALID;
		}

		virtual size_t write(Const_byte_range_ptr const &, Seek)
		{
			Genode::error("Vfs_ram::Node::write() called");
			return 0;
		}

		virtual void truncate(Seek)
		{
			Genode::error("Vfs_ram::Node::truncate() called");
		}


		/************************
		 ** Avl node interface **
		 ************************/

		bool higher(Node *c) { return (strcmp(c->_name, _name) > 0); }

		/**
		 * Find index N by walking down the tree N times,
		 * not the most efficient way to do this.
		 */
		Node *index(size_t &i)
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

		Node *sibling(const char * const name)
		{
			if (strcmp(name, _name) == 0) return this;

			Node * const c =
				Avl_node<Node>::child(strcmp(name, _name) > 0);
			return c ? c->sibling(name) : nullptr;
		}
};


class Vfs_ram::File : public Vfs_ram::Node
{
	private:

		typedef Chunk      <num_level_3_entries()>                Chunk_level_3;
		typedef Chunk_index<num_level_2_entries(), Chunk_level_3> Chunk_level_2;
		typedef Chunk_index<num_level_1_entries(), Chunk_level_2> Chunk_level_1;
		typedef Chunk_index<num_level_0_entries(), Chunk_level_1> Chunk_level_0;

		Chunk_level_0 _chunk;

		size_t _length = 0;

	public:

		File(char const * const name, Allocator &alloc)
		: Node(name), _chunk(alloc, Seek{0}) { }

		size_t read(Byte_range_ptr const &dst, Seek seek) override
		{
			size_t const chunk_used_size = _chunk.used_size();

			if (seek.value >= _length)
				return 0;

			/*
			 * Constrain read transaction to available chunk data
			 *
			 * Note that 'chunk_used_size' may be lower than '_length'
			 * because 'Chunk' may have truncated tailing zeros.
			 */

			size_t const len = (seek.value + dst.num_bytes >= _length)
			                 ? _length - min(_length, seek.value)
			                 : dst.num_bytes;

			size_t read_len = len;

			if (seek.value + read_len > chunk_used_size) {
				if (chunk_used_size >= seek.value)
					read_len = chunk_used_size - seek.value;
				else
					read_len = 0;
			}

			_chunk.read(Byte_range_ptr(dst.start, read_len), seek);

			/* add zero padding if needed */
			if (read_len < dst.num_bytes)
				memset(dst.start + read_len, 0, len - read_len);

			return len;
		}

		Vfs::File_io_service::Read_result complete_read(Byte_range_ptr const &dst,
		                                                Seek seek, size_t &out_count) override
		{
			out_count = read(dst, seek);
			return Vfs::File_io_service::READ_OK;
		}

		size_t write(Const_byte_range_ptr const &src, Seek const seek) override
		{
			size_t const at = (seek.value == ~0UL) ? _chunk.used_size() : seek.value;

			size_t len = src.num_bytes;

			if (at + src.num_bytes >= Chunk_level_0::SIZE)
				len = Chunk_level_0::SIZE - at + src.num_bytes;

			try { _chunk.write(src, Seek{at}); }
			catch (Out_of_memory) { return 0; }

			/*
			 * Keep track of file length. We cannot use 'chunk.used_size()'
			 * as file length because trailing zeros may by represented
			 * by zero chunks, which do not contribute to 'used_size()'.
			 */
			_length = max(_length, at + len);

			return len;
		}

		size_t length() override { return _length; }

		void truncate(Seek size) override
		{
			if (size.value < _chunk.used_size())
				_chunk.truncate(size);

			_length = size.value;
		}
};


class Vfs_ram::Symlink : public Vfs_ram::Node
{
	private:

		char   _target[MAX_PATH_LEN];
		size_t _len = 0;

	public:

		Symlink(char const *name) : Node(name) { }

		size_t length() override { return _len; }

		Vfs::File_io_service::Read_result complete_read(Byte_range_ptr const &dst, Seek,
		                                                size_t &out_count) override
		{
			out_count = min(dst.num_bytes, _len);

			memcpy(dst.start, _target, out_count);

			return Vfs::File_io_service::READ_OK;
		}

		size_t write(Const_byte_range_ptr const &src, Seek) override
		{
			if (src.num_bytes > MAX_PATH_LEN)
				return 0;

			size_t len = src.num_bytes;

			for (size_t i = 0; i < len; ++i) {
				if (src.start[i] == '\0') {
					len = i + 1; /* number of characters + terminating zero */
					break;
				}
			}

			_len = len;
			memcpy(_target, src.start, _len);

			return len;
		}
};


class Vfs_ram::Directory : public Vfs_ram::Node
{
	private:

		Avl_tree<Node> _entries { };

		size_t _count = 0;

	public:

		Directory(char const *name) : Node(name) { }

		void empty(Allocator &alloc)
		{
			while (Node *node = _entries.first()) {
				_entries.remove(node);
				if (File *file = dynamic_cast<File*>(node)) {
					if (file->opened())
						continue;
				} else if (Directory *dir = dynamic_cast<Directory*>(node)) {
					dir->empty(alloc);
				}
				destroy(alloc, node);
			}
		}

		void adopt(Node *node)
		{
			_entries.insert(node);
			++_count;
		}

		Node *child(char const *name)
		{
			Node * const node = _entries.first();
			return node ? node->sibling(name) : nullptr;
		}

		void release(Node *node)
		{
			_entries.remove(node);
			--_count;
		}

		size_t length() override { return _count; }

		Vfs::File_io_service::Read_result complete_read(Byte_range_ptr const &dst,
		                                                Seek const seek,
		                                                size_t &out_count) override
		{
			typedef Vfs::Directory_service::Dirent Dirent;

			if (dst.num_bytes < sizeof(Dirent))
				return Vfs::File_io_service::READ_ERR_INVALID;

			size_t index = seek.value / sizeof(Dirent);

			Dirent &dirent = *(Dirent*)dst.start;

			using Dirent_type = Vfs::Directory_service::Dirent_type;

			out_count = sizeof(Dirent);

			Node *node_ptr = _entries.first();
			if (node_ptr) node_ptr = node_ptr->index(index);
			if (!node_ptr) {
				dirent.type = Dirent_type::END;
				return Vfs::File_io_service::READ_OK;
			}

			Node &node = *node_ptr;

			auto dirent_type = [&] ()
			{
				if (dynamic_cast<File      *>(node_ptr)) return Dirent_type::CONTINUOUS_FILE;
				if (dynamic_cast<Directory *>(node_ptr)) return Dirent_type::DIRECTORY;
				if (dynamic_cast<Symlink   *>(node_ptr)) return Dirent_type::SYMLINK;

				return Dirent_type::END;
			};

			Dirent_type const type = dirent_type();

			if (type == Dirent_type::END)
				return Vfs::File_io_service::READ_ERR_INVALID;

			dirent = {
				.fileno = node.inode,
				.type   = type,
				.rwx    = node.rwx(),
				.name   = { node.name() }
			};

			return Vfs::File_io_service::READ_OK;
		}
};


class Vfs::Ram_file_system : public Vfs::File_system
{
	private:

		friend class Genode::List<Vfs_ram::Watch_handle>;

		Vfs::Env           &_env;
		Vfs_ram::Directory  _root = { "" };

		Vfs_ram::Node *lookup(char const *path, bool return_parent = false)
		{
			using namespace Vfs_ram;

			if (*path ==  '/') ++path;
			if (*path == '\0') return &_root;

			char buf[Vfs::MAX_PATH_LEN];
			copy_cstring(buf, path, Vfs::MAX_PATH_LEN);
			Directory *dir = &_root;

			char *name = &buf[0];
			for (size_t i = 0; i < MAX_PATH_LEN; ++i) {
				if (buf[i] == '/') {
					buf[i] = '\0';

					Node * const node = dir->child(name);
					if (!node) return nullptr;

					dir = dynamic_cast<Directory *>(node);
					if (!dir) return nullptr;

					/* set the current name aside */
					name = &buf[i+1];
				} else if (buf[i] == '\0') {
					if (return_parent)
						return dir;
					else
						return dir->child(name);
				}
			}
			return nullptr;
		}

		Vfs_ram::Directory *lookup_parent(char const *path)
		{
			using namespace Vfs_ram;

			Node * const node = lookup(path, true);
			if (node)
				return dynamic_cast<Directory *>(node);
			return nullptr;
		}

		void remove(Vfs_ram::Node *node)
		{
			using namespace Vfs_ram;

			if (File * const file = dynamic_cast<File*>(node)) {
				if (file->opened()) {
					file->unlink();
					return;
				}
			} else if (Directory *dir = dynamic_cast<Directory*>(node)) {
				dir->empty(_env.alloc());
			}

			destroy(_env.alloc(), node);
		}

	public:

		Ram_file_system(Vfs::Env &env, Genode::Xml_node) : _env(env) { }

		~Ram_file_system() { _root.empty(_env.alloc()); }


		/*********************************
		 ** Directory service interface **
		 *********************************/

		file_size num_dirent(char const *path) override
		{
			using namespace Vfs_ram;

			if (Node * const node = lookup(path))
				if (Directory * const dir = dynamic_cast<Directory *>(node))
					return dir->length();

			return 0;
		}

		bool directory(char const * const path) override
		{
			using namespace Vfs_ram;

			Node * const node = lookup(path);
			return node
				? (dynamic_cast<Directory *>(node) != nullptr)
				: false;
		}

		char const *leaf_path(char const *path) override {
			return lookup(path) ? path : nullptr; }

		Open_result open(char const * const path, unsigned mode,
		                 Vfs_handle **handle, Allocator &alloc) override
		{
			using namespace Vfs_ram;

			File *file;
			char const * const name = basename(path);
			bool const       create = mode & OPEN_MODE_CREATE;

			if (create) {
				Directory * const parent = lookup_parent(path);

				if (!parent)
					return OPEN_ERR_UNACCESSIBLE;

				if (parent->child(name))
					return OPEN_ERR_EXISTS;

				if (strlen(name) >= MAX_NAME_LEN)
					return OPEN_ERR_NAME_TOO_LONG;

				try { file = new (_env.alloc()) File(name, _env.alloc()); }
				catch (Out_of_memory) { return OPEN_ERR_NO_SPACE; }
				parent->adopt(file);
				parent->notify();
			} else {
				Node * const node = lookup(path);
				if (!node) return OPEN_ERR_UNACCESSIBLE;

				file = dynamic_cast<File *>(node);
				if (!file) return OPEN_ERR_UNACCESSIBLE;
			}

			try {
				*handle = new (alloc) Io_handle(*this, alloc, mode, *file);
				return OPEN_OK;
			} catch (Genode::Out_of_ram) {
				if (create) {
					lookup_parent(path)->release(file);
					remove(file);
				}
				return OPEN_ERR_OUT_OF_RAM;
			} catch (Genode::Out_of_caps) {
				if (create) {
					lookup_parent(path)->release(file);
					remove(file);
				}
				return OPEN_ERR_OUT_OF_CAPS;
			}
		}

		Opendir_result opendir(char const * const path, bool create,
		                       Vfs_handle **handle, Allocator &alloc) override
		{
			using namespace Vfs_ram;

			Directory * const parent = lookup_parent(path);
			if (!parent)
				return OPENDIR_ERR_LOOKUP_FAILED;

			char const * const name = basename(path);

			Directory *dir;

			if (create) {
				if (*name == '\0')
					return OPENDIR_ERR_NODE_ALREADY_EXISTS;

				if (strlen(name) >= MAX_NAME_LEN)
					return OPENDIR_ERR_NAME_TOO_LONG;

				if (parent->child(name))
					return OPENDIR_ERR_NODE_ALREADY_EXISTS;

				try { dir = new (_env.alloc()) Directory(name); }
				catch (Out_of_memory) { return OPENDIR_ERR_NO_SPACE; }

				parent->adopt(dir);
				parent->notify();
			} else {

				Node * const node = lookup(path);
				if (!node) return OPENDIR_ERR_LOOKUP_FAILED;

				dir = dynamic_cast<Directory *>(node);
				if (!dir) return OPENDIR_ERR_LOOKUP_FAILED;
			}

			try {
				*handle = new (alloc) Io_handle(
					*this, alloc, Io_handle::STATUS_RDONLY, *dir);
				return OPENDIR_OK;
			} catch (Genode::Out_of_ram) {
				if (create) {
					parent->release(dir);
					remove(dir);
				}
				return OPENDIR_ERR_OUT_OF_RAM;
			} catch (Genode::Out_of_caps) {
				if (create) {
					parent->release(dir);
					remove(dir);
				}
				return OPENDIR_ERR_OUT_OF_CAPS;
			}
		}

		Openlink_result openlink(char const * const path, bool create,
		                         Vfs_handle **handle, Allocator &alloc) override
		{
			using namespace Vfs_ram;

			Directory * const parent = lookup_parent(path);
			if (!parent)
				return OPENLINK_ERR_LOOKUP_FAILED;

			char const * const name = basename(path);

			Symlink *link;

			Node * const node = parent->child(name);

			if (create) {

				if (node)
					return OPENLINK_ERR_NODE_ALREADY_EXISTS;

				if (strlen(name) >= MAX_NAME_LEN)
					return OPENLINK_ERR_NAME_TOO_LONG;

				try { link = new (_env.alloc()) Symlink(name); }
				catch (Out_of_memory) { return OPENLINK_ERR_NO_SPACE; }

				parent->adopt(link);
				parent->notify();
			} else {

				if (!node)
					return OPENLINK_ERR_LOOKUP_FAILED;

				link = dynamic_cast<Symlink *>(node);
				if (!link) return OPENLINK_ERR_LOOKUP_FAILED;
			}

			try {
				*handle = new (alloc)
					Io_handle(*this, alloc, Io_handle::STATUS_RDWR, *link);
				return OPENLINK_OK;
			} catch (Genode::Out_of_ram) {
				if (create) {
					parent->release(link);
					remove(link);
				}
				return OPENLINK_ERR_OUT_OF_RAM;
			} catch (Genode::Out_of_caps) {
				if (create) {
					parent->release(link);
					remove(link);
				}
				return OPENLINK_ERR_OUT_OF_CAPS;
			}
		}

		void close(Vfs_handle *vfs_handle) override
		{
			Vfs_ram::Io_handle * const ram_handle =
				static_cast<Vfs_ram::Io_handle *>(vfs_handle);

			Vfs_ram::Node &node = ram_handle->node;
			bool node_modified = ram_handle->modifying;

			node.close(*ram_handle);
			destroy(vfs_handle->alloc(), ram_handle);

			if (node.unlinked() && !node.opened()) {
				destroy(_env.alloc(), &node);
			} else if (node_modified) {
				node.notify();
			}
		}

		Stat_result stat(char const *path, Stat &stat) override
		{
			using namespace Vfs_ram;

			Node * const node_ptr = lookup(path);
			if (!node_ptr)
				return STAT_ERR_NO_ENTRY;

			Node &node = *node_ptr;

			auto node_type = [&] ()
			{
				if (dynamic_cast<Directory *>(node_ptr)) return Node_type::DIRECTORY;
				if (dynamic_cast<Symlink   *>(node_ptr)) return Node_type::SYMLINK;

				return Node_type::CONTINUOUS_FILE;
			};

			stat = {
				.size              = node.length(),
				.type              = node_type(),
				.rwx               = node.rwx(),
				.inode             = node.inode,
				.device            = (Genode::addr_t)this,
				.modification_time = node.modification_time()
			};

			return STAT_OK;
		}

		Rename_result rename(char const * const from, char const * const to) override
		{
			using namespace Vfs_ram;

			if ((strcmp(from, to) == 0) && lookup(from))
				return RENAME_OK;

			char const * const new_name = basename(to);
			if (strlen(new_name) >= MAX_NAME_LEN)
				return RENAME_ERR_NO_PERM;

			Directory * const from_dir = lookup_parent(from);
			if (!from_dir)
				return RENAME_ERR_NO_ENTRY;

			Directory * const to_dir = lookup_parent(to);
			if (!to_dir)
				return RENAME_ERR_NO_ENTRY;

			Node * const from_node = from_dir->child(basename(from));
			if (!from_node)
				return RENAME_ERR_NO_ENTRY;

			Node * const to_node = to_dir->child(new_name);
			if (to_node) {

				if (Directory * const dir = dynamic_cast<Directory*>(to_node))
					if (dir->length() || (!dynamic_cast<Directory*>(from_node)))
						return RENAME_ERR_NO_PERM;

				/* detach node to be replaced from directory */
				to_dir->release(to_node);

				/* notify the node being replaced */
				to_node->notify();

				/* free the node that is replaced */
				remove(to_node);
			}

			from_dir->release(from_node);
			from_node->name(new_name);
			to_dir->adopt(from_node);

			from_dir->notify();
			to_dir->notify();

			return RENAME_OK;
		}

		Unlink_result unlink(char const * const path) override
		{
			using namespace Vfs_ram;

			Directory * const parent = lookup_parent(path);
			if (!parent)
				return UNLINK_ERR_NO_ENTRY;

			Node * const node = parent->child(basename(path));
			if (!node)
				return UNLINK_ERR_NO_ENTRY;

			parent->release(node);
			node->notify();
			parent->notify();
			remove(node);
			return UNLINK_OK;
		}

		Dataspace_capability dataspace(char const * const path) override
		{
			using namespace Vfs_ram;

			Ram_dataspace_capability ds_cap;

			Node * const node = lookup(path);
			if (!node)
				return ds_cap;

			File * const file = dynamic_cast<File *>(node);
			if (!file)
				return ds_cap;

			size_t len = file->length();

			char *local_addr = nullptr;
			try {
				ds_cap = _env.env().ram().alloc(len);

				local_addr = _env.env().rm().attach(ds_cap);
				file->read(Byte_range_ptr(local_addr, file->length()), Seek{0});
				_env.env().rm().detach(local_addr);

			} catch(...) {
				_env.env().rm().detach(local_addr);
				_env.env().ram().free(ds_cap);
				return Dataspace_capability();
			}
			return ds_cap;
		}

		void release(char const *, Dataspace_capability ds_cap) override {
			_env.env().ram().free(
				static_cap_cast<Genode::Ram_dataspace>(ds_cap)); }


		Watch_result watch(char const * const path, Vfs_watch_handle **handle,
		                   Allocator &alloc) override
		{
			using namespace Vfs_ram;

			Node * const node = lookup(path);
			if (!node)
				return WATCH_ERR_UNACCESSIBLE;

			try {
				Vfs_ram::Watch_handle * const watch_handle = new(alloc)
					Vfs_ram::Watch_handle(*this, alloc, *node);
				node->open(*watch_handle);
				*handle = watch_handle;
				return WATCH_OK;
			}
			catch (Genode::Out_of_ram)  { return WATCH_ERR_OUT_OF_RAM;  }
			catch (Genode::Out_of_caps) { return WATCH_ERR_OUT_OF_CAPS; }
		}

		void close(Vfs_watch_handle * const vfs_handle) override
		{
			Vfs_ram::Watch_handle * const watch_handle =
				static_cast<Vfs_ram::Watch_handle *>(vfs_handle);
			watch_handle->node.close(*watch_handle);
			destroy(watch_handle->alloc(), watch_handle);
		};


		/************************
		 ** File I/O interface **
		 ************************/

		Write_result write(Vfs_handle * const vfs_handle,
		                   Const_byte_range_ptr const &buf,
		                   size_t &out) override
		{
			if ((vfs_handle->status_flags() & OPEN_MODE_ACCMODE) ==  OPEN_MODE_RDONLY)
				return WRITE_ERR_INVALID;

			Vfs_ram::Io_handle &handle =
				*static_cast<Vfs_ram::Io_handle *>(vfs_handle);

			Vfs_ram::Seek const seek { size_t(handle.seek()) };

			out = handle.node.write(buf, seek);
			handle.modifying = true;

			return WRITE_OK;
		}

		Read_result complete_read(Vfs_handle * const vfs_handle,
		                          Byte_range_ptr const &dst, size_t &out_count) override
		{
			out_count = 0;

			Vfs_ram::Io_handle const &handle =
				*static_cast<Vfs_ram::Io_handle *>(vfs_handle);

			Vfs_ram::Seek const seek { size_t(handle.seek()) };

			return handle.node.complete_read(dst, seek, out_count);
		}

		bool read_ready (Vfs_handle const &) const override { return true; }
		bool write_ready(Vfs_handle const &) const override { return true; }

		Ftruncate_result ftruncate(Vfs_handle * const vfs_handle, file_size len) override
		{
			if ((vfs_handle->status_flags() & OPEN_MODE_ACCMODE) ==  OPEN_MODE_RDONLY)
				return FTRUNCATE_ERR_NO_PERM;

			Vfs_ram::Io_handle const &handle =
				*static_cast<Vfs_ram::Io_handle *>(vfs_handle);

			Vfs_ram::Seek const at { size_t(len) };

			try { handle.node.truncate(at); }
			catch (Vfs_ram::Out_of_memory) { return FTRUNCATE_ERR_NO_SPACE; }
			return FTRUNCATE_OK;
		}

		/**
		 * Notify other handles if this handle has modified the node
		 */
		Sync_result complete_sync(Vfs_handle * const vfs_handle) override
		{
			Vfs_ram::Io_handle &handle =
				*static_cast<Vfs_ram::Io_handle *>(vfs_handle);

			if (handle.modifying) {
				handle.modifying = false;
				handle.node.close(handle);
				handle.node.notify();
				handle.node.open(handle);
			}
			return SYNC_OK;
		}

		bool update_modification_timestamp(Vfs_handle * const vfs_handle,
		                                   Vfs::Timestamp time) override
		{
			if ((vfs_handle->status_flags() & OPEN_MODE_ACCMODE) ==  OPEN_MODE_RDONLY)
				return false;

			Vfs_ram::Io_handle &handle =
				*static_cast<Vfs_ram::Io_handle *>(vfs_handle);

			handle.modifying = true;

			return handle.node.update_modification_timestamp(time);
		}


		/***************************
		 ** File_system interface **
		 ***************************/

		static char const *name()   { return "ram"; }
		char const *type() override { return "ram"; }
};

#endif /* _INCLUDE__VFS__RAM_FILE_SYSTEM_H_ */
