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

#ifndef _INCLUDE__GEMS__VFS_H_
#define _INCLUDE__GEMS__VFS_H_

/* Genode includes */
#include <base/env.h>
#include <base/allocator.h>
#include <vfs/dir_file_system.h>
#include <vfs/file_system_factory.h>

namespace Genode {
	struct Directory;
	struct Root_directory;
	struct File;
	struct Readonly_file;
	struct File_content;
}


struct Genode::Directory : Noncopyable
{
	public:

		struct Open_failed     : Exception { };
		struct Read_dir_failed : Exception { };

		class Entry
		{
			private:

				Vfs::Directory_service::Dirent _dirent;

				friend class Directory;

				Entry() { }

			public:

				void print(Output &out) const
				{
					using Genode::print;
					using Vfs::Directory_service;

					print(out, _dirent.name, " (");
					switch (_dirent.type) {
					case Directory_service::DIRENT_TYPE_FILE:      print(out, "file");    break;
					case Directory_service::DIRENT_TYPE_DIRECTORY: print(out, "dir");     break;
					case Directory_service::DIRENT_TYPE_SYMLINK:   print(out, "symlink"); break;
					default:                                       print(out, "other");   break;
					}
					print(out, ")");
				}

				typedef String<Vfs::Directory_service::DIRENT_MAX_NAME_LEN> Name;

				Name name() const { return Name(Cstring(_dirent.name)); }
		};

		typedef String<256> Path;

	private:

		Path const _path;

		Vfs::File_system &_fs;

		Entrypoint &_ep;

		Allocator &_alloc;

		Vfs::Vfs_handle *_handle = nullptr;

		friend class Readonly_file;
		friend class Root_directory;

		/**
		 * Constructor used by 'Root_directory'
		 *
		 * \throw Open_failed
		 */
		Directory(Vfs::File_system &fs, Entrypoint &ep, Allocator &alloc)
		: _path(""), _fs(fs), _ep(ep), _alloc(alloc)
		{ }

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

		Vfs::Directory_service::Stat _stat(Path const &rel_path) const
		{
			Vfs::Directory_service::Stat stat;

			/*
			 * Ignore return value as the validity of the result is can be
			 * checked by the caller via 'stat.mode != 0'.
			 */
			_nonconst_fs().stat(Path(_path, "/", rel_path).string(), stat);
			return stat;
		}

	public:

		struct Nonexistent_file      : Exception { };
		struct Nonexistent_directory : Exception { };

		/**
		 * Open sub directory
		 *
		 * \throw Nonexistent_directory
		 */
		Directory(Directory &other, Path const &rel_path)
		: _path(other._path, "/", rel_path), _fs(other._fs), _ep(other._ep),
		  _alloc(other._alloc)
		{
			if (_fs.opendir(_path.string(), false, &_handle, _alloc) !=
			    Vfs::Directory_service::OPENDIR_OK)
				throw Nonexistent_directory();
		}

		~Directory() { _handle->ds().close(_handle); }

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

				if (entry._dirent.type == Vfs::Directory_service::DIRENT_TYPE_END)
					return;

				fn(entry);
			}
		}

		bool file_exists(Path const &rel_path) const
		{
			return _stat(rel_path).mode & Vfs::Directory_service::STAT_MODE_FILE;
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
			Vfs::Directory_service::Stat stat = _stat(rel_path);
			if (!(stat.mode & Vfs::Directory_service::STAT_MODE_FILE))
				throw Nonexistent_file();

			return stat.size;
		}
};


struct Genode::Root_directory : public  Vfs::Io_response_handler,
                                private Vfs::Global_file_system_factory,
                                private Vfs::Dir_file_system,
                                public  Directory
{
	void handle_io_response(Vfs::Vfs_handle::Context*) override { }

	Root_directory(Env &env, Allocator &alloc, Xml_node config)
	:
		Vfs::Global_file_system_factory(alloc),
		Vfs::Dir_file_system(env, alloc, config, *this, *this),
		Directory(*this, env.ep(), alloc)
	{ }

	void apply_config(Xml_node config) { Vfs::Dir_file_system::apply_config(config); }
};


struct Genode::File : Noncopyable
{
	struct Open_failed : Exception { };

	struct Truncated_during_read : Exception { };

	typedef Directory::Path Path;
};


class Genode::Readonly_file : public File
{
	private:

		Vfs::Vfs_handle    *_handle = nullptr;
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

	public:

		/**
		 * Constructor
		 *
		 * \throw File::Open_failed
		 */
		Readonly_file(Directory &dir, Path const &rel_path)
		: _ep(dir._ep)
		{
			_open(dir._fs, dir._alloc, Path(dir._path, "/", rel_path));
		}

		~Readonly_file() { _handle->ds().close(_handle); }

		/**
		 * Read number of 'bytes' from file into local memory buffer 'dst'
		 *
		 * \throw Truncated_during_read
		 */
		size_t read(char *dst, size_t bytes)
		{
			Vfs::file_size out_count = 0;

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
};


class Genode::File_content
{
	private:

		Allocator &_alloc;
		size_t const _size;

		char *_buffer = (char *)_alloc.alloc(_size);

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
		File_content(Allocator &alloc, Directory &dir, Path const &rel_path,
		             Limit limit)
		:
			_alloc(alloc),
			_size(min(dir.file_size(rel_path), (Vfs::file_size)limit.value))
		{
			if (Readonly_file(dir, rel_path).read(_buffer, _size) != _size)
				throw Truncated_during_read();
		}

		~File_content() { _alloc.free(_buffer, _size); }

		/**
		 * Call functor 'fn' with content as 'Xml_node' argument
		 *
		 * If the file does not contain valid XML, 'fn' is called with an
		 * '<empty/>' node as argument.
		 */
		template <typename FN>
		void xml(FN const &fn) const
		{
			try { fn(Xml_node(_buffer, _size)); }
			catch (Xml_node::Invalid_syntax) { fn(Xml_node("<empty/>")); }
		}

		/**
		 * Call functor 'fn' with each line of the file as argument
		 *
		 * \param STRING  string type used for the line
		 */
		template <typename STRING, typename FN>
		void for_each_line(FN const &fn) const
		{
			char const *src           = _buffer;
			char const *curr_line     = src;
			size_t      curr_line_len = 0;

			for (size_t n = 0; n < _size; n++) {

				char const c = *src++;
				bool const end_of_data = (c == 0 || n + 1 == _size);
				bool const end_of_line = (c == '\n');

				if (!end_of_data && !end_of_line) {
					curr_line_len++;
					continue;
				}

				fn(STRING(Cstring(curr_line, curr_line_len)));

				if (end_of_data)
					return;

				curr_line     = src;
				curr_line_len = 0;
			}
		}
};

#endif /* _INCLUDE__GEMS__VFS_H_ */
