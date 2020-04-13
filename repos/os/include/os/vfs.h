 /*
 * \brief  Front-end API for accessing a component-local virtual file system
 * \author Norman Feske
 * \date   2017-07-04
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__OS__VFS_H_
#define _INCLUDE__OS__VFS_H_

/* Genode includes */
#include <base/env.h>
#include <base/allocator.h>
#include <vfs/simple_env.h>
#include <vfs/dir_file_system.h>
#include <vfs/file_system_factory.h>

namespace Genode {
	struct Directory;
	struct Root_directory;
	struct File;
	struct Readonly_file;
	struct File_content;
	struct Watcher;
	template <typename>
	struct Watch_handler;
}


struct Genode::Directory : Noncopyable, Interface
{
	public:

		struct Open_failed     : Exception { };
		struct Read_dir_failed : Exception { };

		class Entry
		{
			private:

				Vfs::Directory_service::Dirent _dirent { };

				friend class Directory;

				Entry() { }

				using Dirent_type = Vfs::Directory_service::Dirent_type;

			public:

				void print(Output &out) const
				{
					using Genode::print;
					using Vfs::Directory_service;

					print(out, _dirent.name.buf, " (");
					switch (_dirent.type) {
					case Dirent_type::TRANSACTIONAL_FILE: print(out, "file");    break;
					case Dirent_type::CONTINUOUS_FILE:    print(out, "file");    break;
					case Dirent_type::DIRECTORY:          print(out, "dir");     break;
					case Dirent_type::SYMLINK:            print(out, "symlink"); break;
					default:                              print(out, "other");   break;
					}
					print(out, ")");
				}

				typedef String<Vfs::Directory_service::Dirent::Name::MAX_LEN> Name;

				Name name() const { return Name(Cstring(_dirent.name.buf)); }

				Vfs::Directory_service::Dirent_type type() const { return _dirent.type; }

				bool dir() const { return _dirent.type == Dirent_type::DIRECTORY; }

				Vfs::Node_rwx rwx() const { return _dirent.rwx; }
		};

		enum { MAX_PATH_LEN = 256 };

		typedef String<MAX_PATH_LEN> Path;

		static Path join(Path const &x, Path const &y)
		{
			char const *p = y.string();
			while (*p == '/') ++p;

			if (x == "/")
				return Path("/", p);

			return Path(x, "/", p);
		}

	private:

		/*
		 * Noncopyable
		 */
		Directory(Directory const &);
		Directory &operator = (Directory const &);

		Path const _path;

		Vfs::File_system &_fs;

		Entrypoint &_ep;

		Allocator &_alloc;

		Vfs::Vfs_handle *_handle = nullptr;

		friend class Readonly_file;
		friend class Root_directory;
		friend class Watcher;

		/*
		 * Operations such as 'file_size' that are expected to be 'const' at
		 * the API level, do internally require I/O with the outside world,
		 * with involves non-const access to the VFS. This helper allows a
		 * 'const' method to perform I/O at the VFS.
		 */
		Vfs::File_system &_nonconst_fs() const
		{
			return const_cast<Vfs::File_system &>(_fs);
		}

		Vfs::Directory_service::Stat_result _stat(Path const &rel_path,
		                                          Vfs::Directory_service::Stat &out) const
		{
			return _nonconst_fs().stat(join(_path, rel_path).string(), out);
		}

	public:

		struct Nonexistent_file      : Exception { };
		struct Nonexistent_directory : Exception { };

		/**
		 * Constructor used by 'Root_directory'
		 *
		 * \throw Open_failed
		 */
		Directory(Vfs::Env &vfs_env)
		: _path(""), _fs(vfs_env.root_dir()),
		  _ep(vfs_env.env().ep()), _alloc(vfs_env.alloc())
		{
			if (_fs.opendir("/", false, &_handle, _alloc) !=
			    Vfs::Directory_service::OPENDIR_OK)
				throw Nonexistent_directory();
		}

		/**
		 * Open sub directory
		 *
		 * \throw Nonexistent_directory
		 */
		Directory(Directory const &other, Path const &rel_path)
		: _path(join(other._path, rel_path)), _fs(other._fs), _ep(other._ep),
		  _alloc(other._alloc)
		{
			if (_fs.opendir(_path.string(), false, &_handle, _alloc) !=
			    Vfs::Directory_service::OPENDIR_OK)
				throw Nonexistent_directory();
		}

		~Directory() { if (_handle) _handle->ds().close(_handle); }

		template <typename FN>
		void for_each_entry(FN const &fn)
		{
			for (unsigned i = 0;; i++) {

				Entry entry;

				_handle->seek(i * sizeof(entry._dirent));

				while (!_handle->fs().queue_read(_handle, sizeof(entry._dirent)))
					_ep.wait_and_dispatch_one_io_signal();

				Vfs::File_io_service::Read_result read_result;
				Vfs::file_size                    out_count = 0;

				for (;;) {

					read_result = _handle->fs().complete_read(_handle,
					                                          (char*)&entry._dirent,
					                                          sizeof(entry._dirent),
					                                          out_count);

					if (read_result != Vfs::File_io_service::READ_QUEUED)
						break;

					_ep.wait_and_dispatch_one_io_signal();
				}

				if ((read_result != Vfs::File_io_service::READ_OK) ||
				    (out_count < sizeof(entry._dirent))) {
					error("could not access directory '", _path, "'");
					throw Read_dir_failed();
				}

				if (entry._dirent.type == Vfs::Directory_service::Dirent_type::END)
					return;

				fn(entry);
			}
		}

		template <typename FN>
		void for_each_entry(FN const &fn) const
		{
			auto const_fn = [&] (Entry const &e) { fn(e); };
			const_cast<Directory &>(*this).for_each_entry(const_fn);
		}

		bool file_exists(Path const &rel_path) const
		{
			Vfs::Directory_service::Stat stat { };

			if (_stat(rel_path, stat) != Vfs::Directory_service::STAT_OK)
				return false;

			return stat.type == Vfs::Node_type::TRANSACTIONAL_FILE
			    || stat.type == Vfs::Node_type::CONTINUOUS_FILE;
		}

		bool directory_exists(Path const &rel_path) const
		{
			Vfs::Directory_service::Stat stat { };

			if (_stat(rel_path, stat) != Vfs::Directory_service::STAT_OK)
				return false;

			return stat.type == Vfs::Node_type::DIRECTORY;
		}

		/**
		 * Return size of file at specified directory-relative path
		 *
		 * \throw Nonexistent_file  file at path does not exist or
		 *                          the access to the file is denied
		 *
		 */
		Vfs::file_size file_size(Path const &rel_path) const
		{
			Vfs::Directory_service::Stat stat { };

			if (_stat(rel_path, stat) != Vfs::Directory_service::STAT_OK)
				throw Nonexistent_file();

			if (stat.type == Vfs::Node_type::TRANSACTIONAL_FILE
			 || stat.type == Vfs::Node_type::CONTINUOUS_FILE)
				return stat.size;

			throw Nonexistent_file();
		}

		/**
		 * Return symlink content at specified directory-relative path
		 *
		 * \throw Nonexistent_file  symlink at path does not exist or
		 *                          access is denied
		 *
		 */
		Path read_symlink(Path const &rel_path) const
		{
			using namespace Vfs;
			Vfs_handle *link_handle;

			auto open_res = _nonconst_fs().openlink(
				join(_path, rel_path).string(),
				false, &link_handle, _alloc);
			if (open_res != Directory_service::OPENLINK_OK)
				throw Nonexistent_file();
			Vfs_handle::Guard guard(link_handle);

			char buf[MAX_PATH_LEN];

			Vfs::file_size count = sizeof(buf)-1;
			Vfs::file_size out_count = 0;
			while (!link_handle->fs().queue_read(link_handle, count)) {
				_ep.wait_and_dispatch_one_io_signal();
			}

			File_io_service::Read_result result;

			for (;;) {
				result = link_handle->fs().complete_read(
					link_handle, buf, count, out_count);

				if (result != File_io_service::READ_QUEUED)
					break;

				_ep.wait_and_dispatch_one_io_signal();
			};

			if (result != File_io_service::READ_OK)
				throw Nonexistent_file();

			return Path(Genode::Cstring(buf, out_count));
		}

		void unlink(Path const &rel_path)
		{
			_fs.unlink(join(_path, rel_path).string());
		}
};


struct Genode::Root_directory : public Vfs::Simple_env,
                                public Directory
{
	Root_directory(Genode::Env &env, Allocator &alloc, Xml_node config)
	:
		Vfs::Simple_env(env, alloc, config), Directory((Vfs::Simple_env&)*this)
	{ }

	void apply_config(Xml_node config) { root_dir().apply_config(config); }
};


struct Genode::File : Noncopyable, Interface
{
	struct Open_failed : Exception { };

	struct Truncated_during_read : Exception { };

	typedef Directory::Path Path;
};


class Genode::Readonly_file : public File
{
	private:

		/*
		 * Noncopyable
		 */
		Readonly_file(Readonly_file const &);
		Readonly_file &operator = (Readonly_file const &);

		Vfs::Vfs_handle mutable *_handle = nullptr;

		Genode::Entrypoint &_ep;

		void _open(Vfs::File_system &fs, Allocator &alloc, Path const path)
		{
			Vfs::Directory_service::Open_result res =
				fs.open(path.string(), Vfs::Directory_service::OPEN_MODE_RDONLY,
				        &_handle, alloc);

			if (res != Vfs::Directory_service::OPEN_OK) {
				error("failed to open file '", path, "'");
				throw Open_failed();
			}
		}

		/**
		 * Strip off constness of 'Directory const &'
		 *
		 * Since the 'Readonly_file' API provides an abstraction over the
		 * low-level VFS operations, the intuitive meaning of 'const' is
		 * different between the 'Readonly_file' API and the VFS.
		 *
		 * At the VFS level, opening a file changes the internal state of the
		 * VFS. Hence the operation is non-const. However, the user of the
		 * 'Readonly_file' API expects the constness of a directory to
		 * correspond to whether the directory can be modified or not. In the
		 * case of instantiating a 'Readonly_file', one would expect that a
		 * 'Directory const &' would suffice. The fact that - under the hood -
		 * the 'Readonly_file' has to perform the nonconst 'open' operation at
		 * the VFS is of not of interest.
		 */
		static Directory &_mutable(Directory const &dir)
		{
			return const_cast<Directory &>(dir);
		}

	public:

		/**
		 * Constructor
		 *
		 * \throw File::Open_failed
		 */
		Readonly_file(Directory const &dir, Path const &rel_path)
		: _ep(_mutable(dir)._ep)
		{
			_open(_mutable(dir)._fs, _mutable(dir)._alloc,
			      Directory::join(dir._path, rel_path));
		}

		~Readonly_file() { _handle->ds().close(_handle); }

		struct At { Vfs::file_size value; };

		/**
		 * Read number of 'bytes' from file into local memory buffer 'dst'
		 *
		 * \throw Truncated_during_read
		 */
		size_t read(At at, char *dst, size_t bytes) const
		{
			Vfs::file_size out_count = 0;

			_handle->seek(at.value);

			while (!_handle->fs().queue_read(_handle, bytes))
				_ep.wait_and_dispatch_one_io_signal();

			Vfs::File_io_service::Read_result result;

			for (;;) {
				result = _handle->fs().complete_read(_handle, dst, bytes,
				                                     out_count);

				if (result != Vfs::File_io_service::READ_QUEUED)
					break;

				_ep.wait_and_dispatch_one_io_signal();
			};

			/*
			 * XXX handle READ_ERR_AGAIN, READ_ERR_WOULD_BLOCK, READ_QUEUED
			 */

			if (result != Vfs::File_io_service::READ_OK)
				throw Truncated_during_read();

			return out_count;
		}

		/**
		 * Read number of 'bytes' from the start of the file into local memory
		 * buffer 'dst'
		 *
		 * \throw Truncated_during_read
		 */
		size_t read(char *dst, size_t bytes) const
		{
			return read(At{0}, dst, bytes);
		}
};


class Genode::File_content
{
	private:

		class Buffer
		{
			private:

				/*
				 * Noncopyable
				 */
				Buffer(Buffer const &);
				Buffer &operator = (Buffer const &);

			public:

				Allocator   &alloc;
				size_t const size;
				char * const ptr = size ? (char *)alloc.alloc(size) : nullptr;

				Buffer(Allocator &alloc, size_t size) : alloc(alloc), size(size) { }
				~Buffer() { if (ptr) alloc.free(ptr, size); }

		} _buffer;

	public:

		typedef Directory::Nonexistent_file Nonexistent_file;
		typedef File::Truncated_during_read Truncated_during_read;

		typedef Directory::Path Path;

		struct Limit { size_t value; };

		/**
		 * Constructor
		 *
		 * \throw Nonexistent_file
		 * \throw Truncated_during_read  number of readable bytes differs
		 *                               from file status information
		 */
		File_content(Allocator &alloc, Directory const &dir, Path const &rel_path,
		             Limit limit)
		:
			_buffer(alloc, min(dir.file_size(rel_path), (Vfs::file_size)limit.value))
		{
			if (Readonly_file(dir, rel_path).read(_buffer.ptr, _buffer.size) != _buffer.size)
				throw Truncated_during_read();
		}

		/**
		 * Call functor 'fn' with content as 'Xml_node' argument
		 *
		 * If the file does not contain valid XML, 'fn' is called with an
		 * '<empty/>' node as argument.
		 */
		template <typename FN>
		void xml(FN const &fn) const
		{
			try {
				if (_buffer.size) {
					fn(Xml_node(_buffer.ptr, _buffer.size));
					return;
				}
			}
			catch (Xml_node::Invalid_syntax) { }

			fn(Xml_node("<empty/>"));
		}

		/**
		 * Call functor 'fn' with each line of the file as argument
		 *
		 * \param STRING  string type used for the line
		 */
		template <typename STRING, typename FN>
		void for_each_line(FN const &fn) const
		{
			char const *src           = _buffer.ptr;
			char const *curr_line     = src;
			size_t      curr_line_len = 0;

			for (size_t n = 0; ; n++) {

				char const c = (n == _buffer.size) ? 0 : *src++;
				bool const end_of_data = (c == 0);
				bool const end_of_line = (c == '\n');

				if (!end_of_data && !end_of_line) {
					curr_line_len++;
					continue;
				}

				if (!end_of_data || curr_line_len > 0)
					fn(STRING(Cstring(curr_line, curr_line_len)));

				if (end_of_data)
					break;

				curr_line     = src;
				curr_line_len = 0;
			}
		}

		/**
		 * Call functor 'fn' with the data pointer and size in bytes
		 *
		 * If the buffer has a size of zero, 'fn' is not called.
		 */
		template <typename FN>
		void bytes(FN const &fn) const
		{
			if (_buffer.size)
				fn((char const *)_buffer.ptr, _buffer.size);
		}
};


class Genode::Watcher
{
	private:

		/*
		 * Noncopyable
		 */
		Watcher(Watcher const &);
		Watcher &operator = (Watcher const &);

		Vfs::Vfs_watch_handle mutable *_handle { nullptr };

		void _watch(Vfs::File_system &fs, Allocator &alloc, Directory::Path const path,
		            Vfs::Watch_response_handler &handler)
		{
			Vfs::Directory_service::Watch_result res =
				fs.watch(path.string(), &_handle, alloc);

			if (res == Vfs::Directory_service::WATCH_OK)
				_handle->handler(&handler);
			else
				error("failed to watch '", path, "'");
		}

		static Directory &_mutable(Directory const &dir)
		{
			return const_cast<Directory &>(dir);
		}

	public:

		Watcher(Directory const &dir, Directory::Path const &rel_path,
		        Vfs::Watch_response_handler &handler)
		{
			_watch(_mutable(dir)._fs, _mutable(dir)._alloc,
			       Directory::join(dir._path, rel_path), handler);
		}

		Watcher(Vfs::File_system &fs, Directory::Path const &rel_path,
		        Genode::Allocator &alloc, Vfs::Watch_response_handler &handler)
		{
			_watch(fs, alloc, rel_path, handler);
		}

		~Watcher()
		{
			if (_handle)
				_handle->fs().close(_handle);
		}
};


template <typename T>
class Genode::Watch_handler : public Vfs::Watch_response_handler,
                              private Watcher

{
	private:

		T  &_obj;
		void (T::*_member) ();

	public:

		Watch_handler(Directory &dir, Directory::Path const &rel_path,
		              T &obj, void (T::*member)())
		:
			Watcher(dir, rel_path, *this), _obj(obj), _member(member)
		{ }

		Watch_handler(Vfs::File_system &fs, Directory::Path const &rel_path,
		              Genode::Allocator &alloc, T &obj, void (T::*member)())
		:
			Watcher(fs,rel_path, alloc, *this), _obj(obj), _member(member)
		{ }

		void watch_response() override { (_obj.*_member)(); }
};

#endif /* _INCLUDE__OS__VFS_H_ */
