/*
 * \brief   Libc plugin for accessing a file-system session
 * \author  Norman Feske
 * \date    2012-04-11
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/env.h>
#include <base/printf.h>
#include <file_system_session/connection.h>

/* libc includes */
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* libc plugin interface */
#include <libc-plugin/plugin.h>
#include <libc-plugin/fd_alloc.h>


static bool const verbose = false;


namespace File_system { struct Packet_ref { }; }


namespace {

enum { PATH_MAX_LEN = 256 };


/**
 * Current working directory
 */
struct Cwd
{
	char path[PATH_MAX_LEN];

	Cwd() { Genode::strncpy(path, "/", sizeof(path)); }
};


static Cwd *cwd()
{
	static Cwd cwd_inst;
	return &cwd_inst;
}


struct Canonical_path
{
	char str[PATH_MAX_LEN];

	Canonical_path(char const *pathname)
	{
		/*
		 * If pathname is a relative path, prepend the current working
		 * directory.
		 *
		 * XXX we might consider using Noux' 'Path' class here
		 */
		if (pathname[0] != '/') {
			snprintf(str, sizeof(str), "%s/%s", cwd()->path, pathname);
		} else {
			strncpy(str, pathname, sizeof(str));
		}
	}
};


static File_system::Session *file_system()
{
	static Genode::Allocator_avl tx_buffer_alloc(Genode::env()->heap());
	static File_system::Connection fs(tx_buffer_alloc);
	return &fs;
}


struct Node_handle_guard
{
	File_system::Node_handle handle;

	Node_handle_guard(File_system::Node_handle handle) : handle(handle) { }

	~Node_handle_guard() { file_system()->close(handle); }
};


class Plugin_context : public Libc::Plugin_context,
                       public File_system::Packet_ref
{
	private:

		enum Type { TYPE_FILE, TYPE_DIR, TYPE_SYMLINK };

		Type _type;

		File_system::Node_handle _node_handle;

		/**
		 * Current file position if manually seeked, or ~0 for append mode
		 */
		off_t _seek_offset;

	public:

		bool in_flight;

		Plugin_context(File_system::File_handle handle)
		: _type(TYPE_FILE), _node_handle(handle), _seek_offset(~0), in_flight(false) { }

		Plugin_context(File_system::Dir_handle handle)
		: _type(TYPE_DIR), _node_handle(handle), _seek_offset(0), in_flight(false) { }

		Plugin_context(File_system::Symlink_handle handle)
		: _type(TYPE_SYMLINK), _node_handle(handle), _seek_offset(~0), in_flight(false) { }

		File_system::Node_handle node_handle() const { return _node_handle; }

		/**
		 * Return true of handle is append mode
		 */
		bool is_appending() const { return ~0 == _seek_offset; }

		/**
		 * Set seek offset, switch to non-append mode
		 */
		void seek_offset(size_t seek_offset) { _seek_offset = seek_offset; }

		/**
		 * Return seek offset if handle is in non-append mode
		 */
		off_t seek_offset() const { return _seek_offset; }

		/**
		 * Advance current seek position by 'incr' number of bytes
		 *
		 * This function has no effect if the handle is in append mode.
		 */
		void advance_seek_offset(size_t incr)
		{
			if (!is_appending())
				_seek_offset += incr;
		}

		void infinite_seek_offset()
		{
			_seek_offset = ~0;
		}

		virtual ~Plugin_context() { }
};


static inline Plugin_context *context(Libc::File_descriptor *fd)
{
	return fd->context ? static_cast<Plugin_context *>(fd->context) : 0;
}


static void wait_for_acknowledgement(File_system::Session::Tx::Source &source)
{
	::File_system::Packet_descriptor packet = source.get_acked_packet();
	PDBG("got acknowledgement for packet of size %zd", packet.size());

	static_cast<Plugin_context *>(packet.ref())->in_flight = false;

	source.release_packet(packet);
}


/**
 * Collect pending packet acknowledgements, freeing the space occupied
 * by the packet in the bulk buffer
 *
 * This function should be called prior enqueing new packets into the
 * packet stream to free up space in the bulk buffer.
 */
static void collect_acknowledgements(File_system::Session::Tx::Source &source)
{
	while (source.ack_avail())
		wait_for_acknowledgement(source);
}


static void obtain_stat_for_node(File_system::Node_handle node_handle,
                                 struct stat *buf)
{
	if (!buf)
		return;

	File_system::Status status = file_system()->status(node_handle);

	/*
	 * Translate status information to 'struct stat' format
	 */
	memset(buf, 0, sizeof(struct stat));
	buf->st_size = status.size;

	if (status.is_directory())
		buf->st_mode |= S_IFDIR;
	else if (status.is_symlink())
		buf->st_mode |= S_IFLNK;
	else
		buf->st_mode |= S_IFREG;

	struct tm tm;
	memset(&tm, 0, sizeof(struct tm));

	buf->st_mtime = mktime(&tm);

	if (buf->st_mtime == -1)
		PERR("mktime() returned -1, the file modification time reported by stat() will be incorrect");
}


class Plugin : public Libc::Plugin
{
	private:

		::off_t _file_size(Libc::File_descriptor *fd)
		{
			struct stat stat_buf;
			if (fstat(fd, &stat_buf) == -1)
				return -1;
			return stat_buf.st_size;
		}

	public:

		/**
		 * Constructor
		 */
		Plugin() { }

		~Plugin() { }

		bool supports_chdir(const char *path)
		{
			if (verbose)
				PDBG("path = %s", path);
			return true;
		}

		bool supports_mkdir(const char *path, mode_t)
		{
			if (verbose)
				PDBG("path = %s", path);
			return true;
		}

		bool supports_open(const char *pathname, int flags)
		{
			if (verbose)
				PDBG("pathname = %s", pathname);
			return true;
		}

		bool supports_rename(const char *oldpath, const char *newpath)
		{
			if (verbose)
				PDBG("oldpath = %s, newpath = %s", oldpath, newpath);
			return true;
		}

		bool supports_stat(const char *path)
		{
			if (verbose)
				PDBG("path = %s", path);
			return true;
		}

		bool supports_unlink(const char *path)
		{
			if (verbose)
				PDBG("path = %s", path);
			return true;
		}

		int chdir(const char *path)
		{
			if (*path != '/') {
				PERR("chdir: relative path names not yet supported");
				errno = ENOENT;
				return -1;
			}

			Genode::strncpy(cwd()->path, path, sizeof(cwd()->path));

			/* strip trailing slash if needed */
			char *s = cwd()->path;
			for (; s[0] && s[1]; s++);
			if (s[0] == '/')
				s[0] = 0;

			return 0;
		}

		int close(Libc::File_descriptor *fd)
		{
			/* wait for the completion of all operations of the context */
			while (context(fd)->in_flight) {
				PDBG("wait_for_acknowledgement");
				wait_for_acknowledgement(*file_system()->tx());
			}

			file_system()->close(context(fd)->node_handle());
			return 0;
		}

		int fcntl(Libc::File_descriptor *, int cmd, long arg)
		{
			PDBG("not implemented");
			/* libc's opendir() fails if fcntl() returns -1, so we return 0 here */
			if (verbose)
				PDBG("fcntl() called - not yet implemented");
			return 0;
		}

		int fstat(Libc::File_descriptor *fd, struct stat *buf)
		{
			try {
				obtain_stat_for_node(context(fd)->node_handle(), buf);
				return 0;
			}
			catch (...) {
				struct Unhandled_exception_in_fstat { };
				throw Unhandled_exception_in_fstat();
			}
			return -1;
		}

		int fstatfs(Libc::File_descriptor *, struct statfs *buf)
		{
			PDBG("not implemented");
			/* libc's opendir() fails if _fstatfs() returns -1, so we return 0 here */
			if (verbose)
				PDBG("_fstatfs() called - not yet implemented");
			return 0;
		}

		int fsync(Libc::File_descriptor *fd)
		{
			PDBG("not implemented");
			return -1;
		}

		/*
		 * *basep does not get initialized by the libc and is therefore
		 * useless for determining a specific directory index. This
		 * function uses the plugin-internal seek offset instead.
		 */
		ssize_t getdirentries(Libc::File_descriptor *fd, char *buf,
		                      ::size_t nbytes, ::off_t *basep)
		{
			using namespace File_system;

			if (nbytes < sizeof(struct dirent)) {
				PERR("buf too small");
				return -1;
			}

			Directory_entry entry;
			size_t num_bytes = read(fd, &entry, sizeof(entry));

			/* detect end of directory entries */
			if (num_bytes == 0)
				return 0;

			if (num_bytes != sizeof(entry)) {
				PERR("getdirentries retrieved unexpected directory entry size");
				return -1;
			}

			struct dirent *dirent = (struct dirent *)buf;
			Genode::memset(dirent, 0, sizeof(struct dirent));

			switch (entry.type) {
			case Directory_entry::TYPE_DIRECTORY: dirent->d_type = DT_DIR;  break;
			case Directory_entry::TYPE_FILE:      dirent->d_type = DT_REG;  break;
			case Directory_entry::TYPE_SYMLINK:   dirent->d_type = DT_LNK;  break;
			}

			dirent->d_fileno = 1 + (context(fd)->seek_offset() / sizeof(struct dirent));
			dirent->d_reclen = sizeof(struct dirent);

			Genode::strncpy(dirent->d_name, entry.name, sizeof(dirent->d_name));

			dirent->d_namlen = Genode::strlen(dirent->d_name);

			*basep += sizeof(struct dirent);
			return sizeof(struct dirent);
		}

		::off_t lseek(Libc::File_descriptor *fd, ::off_t offset, int whence)
		{
			switch (whence) {

			case SEEK_SET:
				context(fd)->seek_offset(offset);
				return offset;

			case SEEK_CUR:
				context(fd)->advance_seek_offset(offset);
				if (context(fd)->is_appending())
					return _file_size(fd);
				return context(fd)->seek_offset();

			case SEEK_END:
				if (offset != 0) {
					errno = EINVAL;
					return -1;
				}
				context(fd)->infinite_seek_offset();
				return _file_size(fd);

			default:
				errno = EINVAL;
				return -1;
			}
		}

		int mkdir(const char *path, mode_t mode)
		{
			Canonical_path canonical_path(path);

			try {
				File_system::Dir_handle const handle =
					file_system()->dir(canonical_path.str, true);
				file_system()->close(handle);
				return 0;
			}
			catch (File_system::Permission_denied)   { errno = EPERM; }
			catch (File_system::Node_already_exists) { errno = EEXIST; }
			catch (File_system::Lookup_failed)       { errno = ENOENT; }
			catch (File_system::Name_too_long)       { errno = ENAMETOOLONG; }
			catch (File_system::No_space)            { errno = ENOSPC; }

			return -1;
		}

		Libc::File_descriptor *open(const char *pathname, int flags)
		{
			Canonical_path path(pathname);

			File_system::Mode mode;
			switch (flags & O_ACCMODE) {
			case O_RDONLY: mode = File_system::READ_ONLY;  break;
			case O_WRONLY: mode = File_system::WRITE_ONLY; break;
			case O_RDWR:   mode = File_system::READ_WRITE; break;
			default:       mode = File_system::STAT_ONLY;  break;
			}

			/*
			 * Probe for an existing directory to open
			 */
			try {
				PDBG("open dir '%s'", path.str);
				File_system::Dir_handle const handle =
					file_system()->dir(path.str, false);

				Plugin_context *context = new (Genode::env()->heap())
					Plugin_context(handle);

				return Libc::file_descriptor_allocator()->alloc(this, context);
			} catch (File_system::Lookup_failed) { }

			/*
			 * Determine directory path that contains the node to open
			 */
			unsigned last_slash = 0;
			for (unsigned i = 0; path.str[i]; i++)
				if (path.str[i] == '/')
					last_slash = i;

			char dir_path[256] = "/";
			if (last_slash > 0)
				Genode::strncpy(dir_path, path.str,
				                Genode::min(sizeof(dir_path), last_slash + 1));

			/*
			 * Determine base name
			 */
			char const *basename = path.str + last_slash + 1;

			try {
				/*
				 * Open directory that contains the file to be opened/created
				 */
				File_system::Dir_handle const dir_handle =
				file_system()->dir(dir_path, false);

				Node_handle_guard guard(dir_handle);

				/*
				 * Open or create file
				 */
				bool const create = (flags & O_CREAT) != 0;
				File_system::File_handle const handle =
					file_system()->file(dir_handle, basename, mode, create);

				Plugin_context *context = new (Genode::env()->heap())
					Plugin_context(handle);

				return Libc::file_descriptor_allocator()->alloc(this, context);

			}
			catch (File_system::Lookup_failed) {
				PERR("open(%s) lookup failed", pathname); }
			return 0;
		}

		int rename(const char *oldpath, const char *newpath)
		{
			PDBG("not implemented");
			return -1;
		}

		ssize_t read(Libc::File_descriptor *fd, void *buf, ::size_t count)
		{
			File_system::Session::Tx::Source &source = *file_system()->tx();

			size_t const max_packet_size = source.bulk_buffer_size() / 2;

			size_t remaining_count = count;

			if (context(fd)->seek_offset() == ~0)
				context(fd)->seek_offset(0);

			while (remaining_count) {

				collect_acknowledgements(source);

				size_t curr_packet_size = Genode::min(remaining_count, max_packet_size);

				/*
				 * XXX handle 'Packet_alloc_failed' exception'
				 */
				File_system::Packet_descriptor
					packet(source.alloc_packet(curr_packet_size),
					       static_cast<File_system::Packet_ref *>(context(fd)),
					       context(fd)->node_handle(),
					       File_system::Packet_descriptor::READ,
					       curr_packet_size,
					       context(fd)->seek_offset());

				/* mark context as having an operation in flight */
				context(fd)->in_flight = true;

				/* pass packet to server side */
				source.submit_packet(packet);

				do {
					packet = source.get_acked_packet();
					static_cast<Plugin_context *>(packet.ref())->in_flight = false;
				} while (context(fd)->in_flight);

				context(fd)->in_flight = false;

				/*
				 * XXX check if acked packet belongs to request,
				 *     needed for thread safety
				 */

				size_t read_num_bytes = Genode::min(packet.length(), curr_packet_size);

				/* copy-out payload into destination buffer */
				memcpy(buf, source.packet_content(packet), read_num_bytes);

				source.release_packet(packet);

				/* prepare next iteration */
				context(fd)->advance_seek_offset(read_num_bytes);
				buf = (void *)((Genode::addr_t)buf + read_num_bytes);
				remaining_count -= read_num_bytes;

				/*
				 * If we received less bytes than requested, we reached the end
				 * of the file.
				 */
				if (read_num_bytes < curr_packet_size)
					break;
			}

			return count - remaining_count;
		}

		int stat(const char *pathname, struct stat *buf)
		{
			PDBG("stat %s", pathname);
			Canonical_path path(pathname);

			try {
				File_system::Node_handle const node_handle =
					file_system()->node(path.str);
				Node_handle_guard guard(node_handle);

				obtain_stat_for_node(node_handle, buf);
				return 0;
			}
			catch (File_system::Lookup_failed) {
				PERR("lookup failed");
				errno = ENOENT;
			}
			return -1;
		}

		int unlink(const char *path)
		{
			return -1;
		}

		ssize_t write(Libc::File_descriptor *fd, const void *buf, ::size_t count)
		{
			File_system::Session::Tx::Source &source = *file_system()->tx();

			size_t const max_packet_size = source.bulk_buffer_size() / 2;

			size_t remaining_count = count;

			while (remaining_count) {

				collect_acknowledgements(source);

				size_t curr_packet_size = Genode::min(remaining_count, max_packet_size);

				/*
				 * XXX handle 'Packet_alloc_failed' exception'
				 */
				File_system::Packet_descriptor
					packet(source.alloc_packet(curr_packet_size),
					       static_cast<File_system::Packet_ref *>(context(fd)),
					       context(fd)->node_handle(),
					       File_system::Packet_descriptor::WRITE,
					       curr_packet_size,
					       context(fd)->seek_offset());

				/* mark context as having an operation in flight */
				context(fd)->in_flight = true;

				/* copy-in payload into packet */
				memcpy(source.packet_content(packet), buf, curr_packet_size);

				/* pass packet to server side */
				source.submit_packet(packet);

				/* prepare next iteration */
				context(fd)->advance_seek_offset(curr_packet_size);
				buf = (void *)((Genode::addr_t)buf + curr_packet_size);
				remaining_count -= curr_packet_size;
			}

			PDBG("write returns %zd", count);
			return count;
		}
};


} /* unnamed namespace */


void __attribute__((constructor)) init_libc_fs(void)
{
	PDBG("using the libc_fs plugin");
	static Plugin plugin;
}
