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
	class  Readonly_file;
	class  File_content;
	class  Writeable_file;
	class  Append_file;
	class  New_file;
	class  Watcher;
	template <typename>
	class  Watch_handler;
	template <typename FN>
	void with_raw_file_content(Readonly_file const &,
	                           Byte_range_ptr const &, FN const &);
	template <typename FN>
	void with_xml_file_content(Readonly_file const &,
	                           Byte_range_ptr const &, FN const &);
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
		friend class Writeable_file;
		friend class Append_file;
		friend class New_file;

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

			return Path(Genode::Cstring(buf, (size_t)out_count));
		}

		void unlink(Path const &rel_path)
		{
			_fs.unlink(join(_path, rel_path).string());
		}

		/**
		 * Attempt to create sub directory
		 *
		 * This operation may fail. Its success can be checked by calling
		 * 'directory_exists'.
		 */
		void create_sub_directory(Path const &sub_path)
		{
			using namespace Genode;

			for (size_t sub_path_len = 0; ; sub_path_len++) {

				char const c = sub_path.string()[sub_path_len];

				bool const end_of_path = (c == 0);
				bool const end_of_elem = (c == '/');

				if (!end_of_elem && !end_of_path)
					continue;

				Path path = join(_path, Path(Cstring(sub_path.string(), sub_path_len)));

				if (!directory_exists(path)) {
					Vfs::Vfs_handle *handle_ptr = nullptr;
					(void)_fs.opendir(path.string(), true, &handle_ptr, _alloc);
					if (handle_ptr)
						handle_ptr->close();
				}

				if (end_of_path)
					break;

				/* skip '/' */
				sub_path_len++;
			}
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

			return (size_t)out_count;
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


/**
 * Call functor 'fn' with the data pointer and size in bytes
 *
 * If the buffer has a size of zero, 'fn' is not called.
 */
template <typename FN>
void Genode::with_raw_file_content(Readonly_file const &file,
                                   Byte_range_ptr const &range, FN const &fn)
{
	if (range.num_bytes == 0)
		return;

	size_t total_read = 0;
	while (total_read < range.num_bytes) {
		size_t read_bytes = file.read(Readonly_file::At{total_read},
				                      range.start  + total_read,
				                      range.num_bytes - total_read);

		if (read_bytes == 0)
			break;

		total_read += read_bytes;
	}

	if (total_read != range.num_bytes)
		throw File::Truncated_during_read();

	fn(range.start, range.num_bytes);
}


/**
 * Call functor 'fn' with content as 'Xml_node' argument
 *
 * If the file does not contain valid XML, 'fn' is called with an
 * '<empty/>' node as argument.
 */
template <typename FN>
void Genode::with_xml_file_content(Readonly_file const &file,
                                   Byte_range_ptr const &range, FN const &fn)
{
	with_raw_file_content(file, range,
	                      [&] (char const *ptr, size_t num_bytes) {
		try {
			fn(Xml_node(ptr, num_bytes));
			return;
		}
		catch (Xml_node::Invalid_syntax) { }

		fn(Xml_node("<empty/>"));
	});
}


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
			_buffer(alloc, min((size_t)dir.file_size(rel_path), limit.value))
		{
			/* read the file content into the buffer */
			with_raw_file_content(Readonly_file(dir, rel_path),
			                      Byte_range_ptr(_buffer.ptr, _buffer.size),
			                      [] (char const*, size_t) { });
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


/**
 * Base class of `New_file` and `Append_file` providing open for write, sync,
 * and append functionality.
 */
class Genode::Writeable_file : Noncopyable
{
	public:

		struct Create_failed : Exception { };

		enum class Append_result { OK, WRITE_ERROR };

	protected:

		static Vfs::Vfs_handle &_init_handle(Directory             &dir,
		                                     Directory::Path const &rel_path)
		{
			/* create compound directory */
			{
				Genode::Path<Vfs::MAX_PATH_LEN> dir_path { rel_path };
				dir_path.strip_last_element();
				dir.create_sub_directory(dir_path.string());
			}

			unsigned mode = Vfs::Directory_service::OPEN_MODE_WRONLY;

			Directory::Path const path = Directory::join(dir._path, rel_path);

			if (!dir.file_exists(path))
				mode |= Vfs::Directory_service::OPEN_MODE_CREATE;

			Vfs::Vfs_handle *handle_ptr = nullptr;
			Vfs::Directory_service::Open_result const res =
				dir._fs.open(path.string(), mode, &handle_ptr, dir._alloc);

			if (res != Vfs::Directory_service::OPEN_OK || (handle_ptr == nullptr)) {
				error("failed to create/open file '", path, "' for writing, res=", (int)res);
				throw Create_failed();
			}

			return *handle_ptr;
		}

		static void _sync(Vfs::Vfs_handle &handle, Entrypoint &ep)
		{
			while (handle.fs().queue_sync(&handle) == false)
				ep.wait_and_dispatch_one_io_signal();

			for (bool sync_done = false; !sync_done; ) {

				switch (handle.fs().complete_sync(&handle)) {

				case Vfs::File_io_service::SYNC_QUEUED:
					break;

				case Vfs::File_io_service::SYNC_ERR_INVALID:
					warning("could not complete file sync operation");
					sync_done = true;
					break;

				case Vfs::File_io_service::SYNC_OK:
					sync_done = true;
					break;
				}

				if (!sync_done)
					ep.wait_and_dispatch_one_io_signal();
			}
		}

		static Append_result _append(Vfs::Vfs_handle &handle, Entrypoint &ep,
		                             char const *src, size_t size)
		{
			bool write_error = false;

			size_t remaining_bytes = size;

			while (remaining_bytes > 0 && !write_error) {

				bool stalled = false;

				try {
					Vfs::file_size out_count = 0;

					using Write_result = Vfs::File_io_service::Write_result;

					switch (handle.fs().write(&handle, src, remaining_bytes,
					                           out_count)) {

					case Write_result::WRITE_ERR_AGAIN:
					case Write_result::WRITE_ERR_WOULD_BLOCK:
						stalled = true;
						break;

					case Write_result::WRITE_ERR_INVALID:
					case Write_result::WRITE_ERR_IO:
					case Write_result::WRITE_ERR_INTERRUPT:
						write_error = true;
						break;

					case Write_result::WRITE_OK:
						out_count = min((Vfs::file_size)remaining_bytes, out_count);
						remaining_bytes -= (size_t)out_count;
						src             += out_count;
						handle.advance_seek(out_count);
						break;
					};
				}
				catch (Vfs::File_io_service::Insufficient_buffer) {
					stalled = true; }

				if (stalled)
					ep.wait_and_dispatch_one_io_signal();
			}
			return write_error ? Append_result::WRITE_ERROR
			                   : Append_result::OK;
		}
};


/**
 * Utility for appending data to an existing file via the Genode VFS library
 */
class Genode::Append_file : public Writeable_file
{
	private:

		Entrypoint       &_ep;
		Vfs::Vfs_handle  &_handle;

	public:

		/**
		 * Constructor
		 *
		 * \throw Create_failed
		 */
		Append_file(Directory &dir, Directory::Path const &path)
		:
			_ep(dir._ep),
			_handle(_init_handle(dir, path))
		{
			Vfs::Directory_service::Stat stat { };
			if (_handle.ds().stat(path.string(), stat) == Vfs::Directory_service::STAT_OK)
				_handle.seek(stat.size);
		}

		~Append_file()
		{
			_sync(_handle, _ep);
			_handle.ds().close(&_handle);
		}

		Append_result append(char const *src, size_t size) {
			return _append(_handle, _ep, src, size); }
};


/**
 * Utility for writing data to a new file via the Genode VFS library
 */
class Genode::New_file : public Writeable_file
{
	private:

		Entrypoint       &_ep;
		Vfs::Vfs_handle  &_handle;

	public:

		using Writeable_file::Append_result;
		using Writeable_file::Create_failed;

		/**
		 * Constructor
		 *
		 * \throw Create_failed
		 */
		New_file(Directory &dir, Directory::Path const &path)
		:
			_ep(dir._ep),
			_handle(_init_handle(dir, path))
		{ _handle.fs().ftruncate(&_handle, 0); }

		~New_file()
		{
			_sync(_handle, _ep);
			_handle.ds().close(&_handle);
		}

		Append_result append(char const *src, size_t size) {
			return _append(_handle, _ep, src, size); }
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

		Watch_handler(Directory const &dir, Directory::Path const &rel_path,
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
