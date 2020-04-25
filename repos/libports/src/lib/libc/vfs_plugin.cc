/*
 * \brief  Libc plugin for using a process-local virtual file system
 * \author Norman Feske
 * \author Christian Helmuth
 * \author Emery Hemingway
 * \date   2014-04-09
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/env.h>
#include <base/log.h>
#include <vfs/dir_file_system.h>

/* libc includes */
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/disk.h>
#include <dlfcn.h>

/* libc plugin interface */
#include <libc-plugin/plugin.h>

/* libc-internal includes */
#include <internal/vfs_plugin.h>
#include <internal/mem_alloc.h>
#include <internal/errno.h>
#include <internal/init.h>
#include <internal/legacy.h>
#include <internal/suspend.h>


static Libc::Suspend *_suspend_ptr;


void Libc::init_vfs_plugin(Suspend &suspend)
{
	_suspend_ptr = &suspend;
}


static void suspend(Libc::Suspend_functor &check)
{
	struct Missing_call_of_init_vfs_plugin : Genode::Exception { };
	if (!_suspend_ptr)
		throw Missing_call_of_init_vfs_plugin();

	_suspend_ptr->suspend(check);
};


static Genode::Lock &vfs_lock()
{
	static Genode::Lock _vfs_lock;
	return _vfs_lock;
}


#define VFS_THREAD_SAFE(code) ({ \
	Genode::Lock::Guard g(vfs_lock()); \
	code; \
})


static Vfs::Vfs_handle *vfs_handle(Libc::File_descriptor *fd)
{
	return reinterpret_cast<Vfs::Vfs_handle *>(fd->context);
}


static Libc::Plugin_context *vfs_context(Vfs::Vfs_handle *vfs_handle)
{
	return reinterpret_cast<Libc::Plugin_context *>(vfs_handle);
}


/**
 * Utility to convert VFS stat struct to the libc stat struct
 *
 * Code shared between 'stat' and 'fstat'.
 */
static void vfs_stat_to_libc_stat_struct(Vfs::Directory_service::Stat const &src,
                                         struct stat *dst)
{
	enum { FS_BLOCK_SIZE = 4096 };

	unsigned const readable_bits   = S_IRUSR,
	               writeable_bits  = S_IWUSR,
	               executable_bits = S_IXUSR;

	auto type = [] (Vfs::Node_type type)
	{
		switch (type) {
		case Vfs::Node_type::DIRECTORY:          return S_IFDIR;
		case Vfs::Node_type::CONTINUOUS_FILE:    return S_IFREG;
		case Vfs::Node_type::TRANSACTIONAL_FILE: return S_IFSOCK;
		case Vfs::Node_type::SYMLINK:            return S_IFLNK;
		}
		return 0;
	};

	*dst = { };

	dst->st_uid     = 0;
	dst->st_gid     = 0;
	dst->st_mode    = (src.rwx.readable   ? readable_bits   : 0)
	                | (src.rwx.writeable  ? writeable_bits  : 0)
	                | (src.rwx.executable ? executable_bits : 0)
	                | type(src.type);
	dst->st_size    = src.size;
	dst->st_blksize = FS_BLOCK_SIZE;
	dst->st_blocks  = (dst->st_size + FS_BLOCK_SIZE - 1) / FS_BLOCK_SIZE;
	dst->st_ino     = src.inode;
	dst->st_dev     = src.device;
	long long mtime = src.modification_time.value;
	dst->st_mtime   = mtime != Vfs::Timestamp::INVALID ? mtime : 0;
	dst->st_nlink   = 1;
}


static Genode::Xml_node *_config_node;

char const *libc_resolv_path;


namespace Libc {

	Xml_node config() __attribute__((weak));
	Xml_node config()
	{
		if (!_config_node) {
			error("libc config not initialized - aborting");
			exit(1);
		}
		return *_config_node;
	}

	class Config_attr
	{
		private:

			typedef String<Vfs::MAX_PATH_LEN> Value;
			Value const _value;

		public:

			Config_attr(char const *attr_name, char const *default_value)
			:
				_value(config().attribute_value(attr_name,
				                                Value(default_value)))
			{ }

			char const *string() const { return _value.string(); }
	};

	char const *config_pipe() __attribute__((weak));
	char const *config_pipe()
	{
		static Config_attr attr("pipe", "");
		return attr.string();
	}

	char const *config_rng() __attribute__((weak));
	char const *config_rng()
	{
		static Config_attr rng("rng", "");
		return rng.string();
	}

	char const *config_socket() __attribute__((weak));
	char const *config_socket()
	{
		static Config_attr socket("socket", "");
		return socket.string();
	}

	char const *config_nameserver_file() __attribute__((weak));
	char const *config_nameserver_file()
	{
		static Config_attr ns_file("nameserver_file",
		                           "/socket/nameserver");
		return ns_file.string();
	}

	void libc_config_init(Xml_node node)
	{
		static Xml_node config = node;
		_config_node = &config;

		libc_resolv_path = config_nameserver_file();
	}

	void notify_read_ready(Vfs::Vfs_handle *handle)
	{
		/*
		 * If this call fails, the VFS plugin is expected to call the IO
		 * handler when the notification request can be processed. The
		 * libc IO handler will then call 'notify_read_ready()' again
		 * via 'select_notify()'.
		 */
		VFS_THREAD_SAFE(handle->fs().notify_read_ready(handle));
	}

	bool read_ready(File_descriptor *fd)
	{
		Vfs::Vfs_handle *handle = vfs_handle(fd);
		if (!handle) return false;

		notify_read_ready(handle);

		return VFS_THREAD_SAFE(handle->fs().read_ready(handle));
	}
}


template <typename FN>
void Libc::Vfs_plugin::_with_info(File_descriptor &fd, FN const &fn)
{
	if (!_root_dir.constructed())
		return;

	Absolute_path path = ioctl_dir(fd);
	path.append_element("info");

	try {
		Lock::Guard g(vfs_lock());

		File_content const content(_alloc, *_root_dir, path.string(),
		                           File_content::Limit{4096U});

		content.xml([&] (Xml_node node) {
			fn(node); });

	} catch (...) { }
}


int Libc::Vfs_plugin::access(const char *path, int amode)
{
	if (VFS_THREAD_SAFE(_root_fs.leaf_path(path)))
		return 0;

	errno = ENOENT;
	return -1;
}


Libc::File_descriptor *Libc::Vfs_plugin::open(char const *path, int flags,
                                              int libc_fd)
{
	if (VFS_THREAD_SAFE(_root_fs.directory(path))) {

		if (((flags & O_ACCMODE) != O_RDONLY)) {
			errno = EISDIR;
			return nullptr;
		}

		flags |= O_DIRECTORY;

		Vfs::Vfs_handle *handle = 0;

		typedef Vfs::Directory_service::Opendir_result Opendir_result;

		switch (VFS_THREAD_SAFE(_root_fs.opendir(path, false, &handle, _alloc))) {
		case Opendir_result::OPENDIR_OK:                      break;
		case Opendir_result::OPENDIR_ERR_LOOKUP_FAILED:       errno = ENOENT;       return nullptr;
		case Opendir_result::OPENDIR_ERR_NAME_TOO_LONG:       errno = ENAMETOOLONG; return nullptr;
		case Opendir_result::OPENDIR_ERR_NODE_ALREADY_EXISTS: errno = EEXIST;       return nullptr;
		case Opendir_result::OPENDIR_ERR_NO_SPACE:            errno = ENOSPC;       return nullptr;
		case Opendir_result::OPENDIR_ERR_OUT_OF_RAM:
		case Opendir_result::OPENDIR_ERR_OUT_OF_CAPS:
		case Opendir_result::OPENDIR_ERR_PERMISSION_DENIED:   errno = EPERM;        return nullptr;
		}

		/* the directory was successfully opened */

		File_descriptor *fd =
			file_descriptor_allocator()->alloc(this, vfs_context(handle), libc_fd);

		/* FIXME error cleanup code leaks resources! */

		if (!fd) {
			VFS_THREAD_SAFE(handle->close());
			errno = EMFILE;
			return nullptr;
		}

		handle->handler(&_response_handler);
		fd->flags = flags & O_ACCMODE;

		return fd;
	}

	if (flags & O_DIRECTORY) {
		errno = ENOTDIR;
		return nullptr;
	}

	typedef Vfs::Directory_service::Open_result Result;

	Vfs::Vfs_handle *handle = 0;

	while (handle == nullptr) {

		switch (VFS_THREAD_SAFE(_root_fs.open(path, flags, &handle, _alloc))) {

		case Result::OPEN_OK:
			break;

		case Result::OPEN_ERR_UNACCESSIBLE:
			{
				if (!(flags & O_CREAT)) {
					if (flags & O_NOFOLLOW) {
					        errno = ELOOP;
					        return 0;
					}
					errno = ENOENT;
					return 0;
				}

				/* O_CREAT is set, so try to create the file */
				switch (VFS_THREAD_SAFE(_root_fs.open(path, flags | O_EXCL, &handle, _alloc))) {

				case Result::OPEN_OK:
					break;

				case Result::OPEN_ERR_EXISTS:

					/* file has been created by someone else in the meantime */
					if (flags & O_NOFOLLOW) {
					        errno = ELOOP;
					        return 0;
					}
					errno = EEXIST;
					return 0;

				case Result::OPEN_ERR_NO_PERM:       errno = EPERM;        return 0;
				case Result::OPEN_ERR_UNACCESSIBLE:  errno = ENOENT;       return 0;
				case Result::OPEN_ERR_NAME_TOO_LONG: errno = ENAMETOOLONG; return 0;
				case Result::OPEN_ERR_NO_SPACE:      errno = ENOSPC;       return 0;
				case Result::OPEN_ERR_OUT_OF_RAM:    errno = ENOSPC;       return 0;
				case Result::OPEN_ERR_OUT_OF_CAPS:   errno = ENOSPC;       return 0;
				}
			}
			break;

		case Result::OPEN_ERR_NO_PERM:       errno = EPERM;        return 0;
		case Result::OPEN_ERR_EXISTS:        errno = EEXIST;       return 0;
		case Result::OPEN_ERR_NAME_TOO_LONG: errno = ENAMETOOLONG; return 0;
		case Result::OPEN_ERR_NO_SPACE:      errno = ENOSPC;       return 0;
		case Result::OPEN_ERR_OUT_OF_RAM:    errno = ENOSPC;       return 0;
		case Result::OPEN_ERR_OUT_OF_CAPS:   errno = ENOSPC;       return 0;
		}
	}

	/* the file was successfully opened */

	File_descriptor *fd =
		file_descriptor_allocator()->alloc(this, vfs_context(handle), libc_fd);

	/* FIXME error cleanup code leaks resources! */

	if (!fd) {
		VFS_THREAD_SAFE(handle->close());
		errno = EMFILE;
		return nullptr;
	}

	handle->handler(&_response_handler);
	fd->flags = flags & (O_ACCMODE|O_NONBLOCK|O_APPEND);

	if ((flags & O_TRUNC) && (ftruncate(fd, 0) == -1)) {
		VFS_THREAD_SAFE(handle->close());
		errno = EINVAL; /* XXX which error code fits best ? */
		return nullptr;
	}

	return fd;
}


void Libc::Vfs_plugin::_vfs_write_mtime(Vfs::Vfs_handle &handle)
{
	struct timespec ts;

	if (_update_mtime == Update_mtime::NO)
		return;

	/* XXX using  clock_gettime directly is probably not the best idea */
	if (clock_gettime(CLOCK_REALTIME, &ts) < 0) {
		ts.tv_sec = 0;
	}

	Vfs::Timestamp time { .value = (long long)ts.tv_sec };

	struct Check : Libc::Suspend_functor
	{
		bool retry { false };

		Vfs::Vfs_handle &vfs_handle;
		Vfs::Timestamp &time;

		Check(Vfs::Vfs_handle &vfs_handle, Vfs::Timestamp &time)
		: vfs_handle(vfs_handle), time(time) { }

		bool suspend() override
		{
			retry = !vfs_handle.fs().update_modification_timestamp(&vfs_handle, time);
			return retry;
		}
	} check(handle, time);

	do {
		_suspend_ptr->suspend(check);
	} while (check.retry);
}


int Libc::Vfs_plugin::_vfs_sync(Vfs::Vfs_handle &vfs_handle)
{
	_vfs_write_mtime(vfs_handle);

	typedef Vfs::File_io_service::Sync_result Result;
	Result result = Result::SYNC_QUEUED;

	{
		struct Check : Suspend_functor
		{
			bool retry { false };

			Vfs::Vfs_handle &vfs_handle;

			Check(Vfs::Vfs_handle &vfs_handle)
			: vfs_handle(vfs_handle) { }

			bool suspend() override
			{
				retry = !VFS_THREAD_SAFE(vfs_handle.fs().queue_sync(&vfs_handle));
				return retry;
			}
		} check(vfs_handle);

		/*
		 * Cannot call suspend() immediately, because the Libc kernel
		 * might not be running yet.
		 */
		if (!VFS_THREAD_SAFE(vfs_handle.fs().queue_sync(&vfs_handle))) {
			do {
				suspend(check);
			} while (check.retry);
		}
	}

	{
		struct Check : Suspend_functor
		{
			bool retry { false };

			Vfs::Vfs_handle &vfs_handle;
			Result          &result;

			Check(Vfs::Vfs_handle &vfs_handle, Result &result)
			: vfs_handle(vfs_handle), result(result) { }

			bool suspend() override
			{
				result = VFS_THREAD_SAFE(vfs_handle.fs().complete_sync(&vfs_handle));
				retry = result == Vfs::File_io_service::SYNC_QUEUED;
				return retry;
			}
		} check(vfs_handle, result);

		/*
		 * Cannot call suspend() immediately, because the Libc kernel
		 * might not be running yet.
		 */
		result = VFS_THREAD_SAFE(vfs_handle.fs().complete_sync(&vfs_handle));
		if (result == Result::SYNC_QUEUED) {
			do {
				suspend(check);
			} while (check.retry);
		}
	}

	return result == Result::SYNC_OK ? 0 : Errno(EIO);
}


int Libc::Vfs_plugin::close(File_descriptor *fd)
{
	Vfs::Vfs_handle *handle = vfs_handle(fd);

	if ((fd->modified) || (fd->flags & O_CREAT)) {
		_vfs_sync(*handle);
	}

	VFS_THREAD_SAFE(handle->close());
	file_descriptor_allocator()->free(fd);
	return 0;
}


int Libc::Vfs_plugin::dup2(File_descriptor *fd,
                           File_descriptor *new_fd)
{
	Vfs::Vfs_handle *handle = nullptr;

	typedef Vfs::Directory_service::Open_result Result;

	if (VFS_THREAD_SAFE(_root_fs.open(fd->fd_path, fd->flags, &handle, _alloc))
	    != Result::OPEN_OK) {

		warning("dup2 failed for path ", fd->fd_path);
		return Errno(EBADF);
	}

	handle->seek(vfs_handle(fd)->seek());
	handle->handler(&_response_handler);

	new_fd->context = vfs_context(handle);
	new_fd->flags = fd->flags;
	new_fd->path(fd->fd_path);

	return new_fd->libc_fd;
}


Libc::File_descriptor *Libc::Vfs_plugin::dup(File_descriptor *fd)
{
	Vfs::Vfs_handle *handle = nullptr;

	typedef Vfs::Directory_service::Open_result Result;

	if (VFS_THREAD_SAFE(_root_fs.open(fd->fd_path, fd->flags, &handle, _alloc))
	    != Result::OPEN_OK) {

		warning("dup failed for path ", fd->fd_path);
		errno = EBADF;
		return nullptr;
	}

	handle->seek(vfs_handle(fd)->seek());
	handle->handler(&_response_handler);

	File_descriptor * const new_fd =
		file_descriptor_allocator()->alloc(this, vfs_context(handle));

	new_fd->flags = fd->flags;
	new_fd->path(fd->fd_path);

	return new_fd;
}


int Libc::Vfs_plugin::fstat(File_descriptor *fd, struct stat *buf)
{
	Vfs::Vfs_handle *handle = vfs_handle(fd);
	if (fd->modified) {
		_vfs_sync(*handle);
		fd->modified = false;
	}

	int const result = stat(fd->fd_path, buf);

	/*
	 * The libc expects stdout to be a character device.
	 * If 'st_mode' is set to 'S_IFREG', 'printf' does not work.
	 */
	if (fd->libc_fd == 1) {
		buf->st_mode &= ~S_IFMT;
		buf->st_mode |=  S_IFCHR;
	}
	return result;
}


int Libc::Vfs_plugin::fstatfs(File_descriptor *fd, struct statfs *buf)
{
	if (!fd || !buf)
		return Errno(EFAULT);

	Genode::memset(buf, 0, sizeof(*buf));

	buf->f_flags = MNT_UNION;
	return 0;
}


int Libc::Vfs_plugin::mkdir(const char *path, mode_t mode)
{
	Vfs::Vfs_handle *dir_handle { 0 };

	typedef Vfs::Directory_service::Opendir_result Opendir_result;

	switch (VFS_THREAD_SAFE(_root_fs.opendir(path, true, &dir_handle, _alloc))) {
	case Opendir_result::OPENDIR_OK:
		VFS_THREAD_SAFE(dir_handle->close());
		break;
	case Opendir_result::OPENDIR_ERR_LOOKUP_FAILED:
		return Errno(ENOENT);
	case Opendir_result::OPENDIR_ERR_NAME_TOO_LONG:
		return Errno(ENAMETOOLONG);
	case Opendir_result::OPENDIR_ERR_NODE_ALREADY_EXISTS:
		return Errno(EEXIST);
	case Opendir_result::OPENDIR_ERR_NO_SPACE:
		return Errno(ENOSPC);
	case Opendir_result::OPENDIR_ERR_OUT_OF_RAM:
	case Opendir_result::OPENDIR_ERR_OUT_OF_CAPS:
	case Opendir_result::OPENDIR_ERR_PERMISSION_DENIED:
		return Errno(EPERM);
	}

	return 0;
}


int Libc::Vfs_plugin::stat(char const *path, struct stat *buf)
{
	if (!path or !buf) {
		errno = EFAULT;
		return -1;
	}

	typedef Vfs::Directory_service::Stat_result Result;

	Vfs::Directory_service::Stat stat;

	switch (VFS_THREAD_SAFE(_root_fs.stat(path, stat))) {
	case Result::STAT_ERR_NO_ENTRY: errno = ENOENT; return -1;
	case Result::STAT_ERR_NO_PERM:  errno = EACCES; return -1;
	case Result::STAT_OK:                           break;
	}

	vfs_stat_to_libc_stat_struct(stat, buf);
	return 0;
}


ssize_t Libc::Vfs_plugin::write(File_descriptor *fd, const void *buf,
                                ::size_t count)
{
	typedef Vfs::File_io_service::Write_result Result;

	if ((fd->flags & O_ACCMODE) == O_RDONLY) {
		return Errno(EBADF);
	}

	Vfs::Vfs_handle *handle = vfs_handle(fd);

	Vfs::file_size out_count  = 0;
	Result         out_result = Result::WRITE_OK;

	if (fd->flags & O_NONBLOCK) {

		try {
			out_result = VFS_THREAD_SAFE(handle->fs().write(handle, (char const *)buf, count, out_count));

			Plugin::resume_all();

		} catch (Vfs::File_io_service::Insufficient_buffer) { }

	} else {

		Vfs::file_size const initial_seek { handle->seek() };

		struct Check : Suspend_functor
		{
			bool retry { false };

			Vfs::File_system  &_root_fs;
			char const * const _fd_path;
			Vfs::Vfs_handle   *_handle;
			void const        *_buf;
			::size_t           _count;
			::off_t            _offset = 0;
			Vfs::file_size    &_out_count;
			Result            &_out_result;
			unsigned           _iteration = 0;

			bool _fd_refers_to_continuous_file()
			{
				if (!_fd_path) {
					warning("Vfs_plugin: _fd_refers_to_continuous_file: missing fd_path");
					return false;
				}

				typedef Vfs::Directory_service::Stat_result Result;

				Vfs::Directory_service::Stat stat { };

				if (VFS_THREAD_SAFE(_root_fs.stat(_fd_path, stat)) != Result::STAT_OK)
					return false;

				return stat.type == Vfs::Node_type::CONTINUOUS_FILE;
			};

			Check(Vfs::File_system &root_fs, char const *fd_path,
			      Vfs::Vfs_handle *handle, void const *buf,
			      ::size_t count, Vfs::file_size &out_count,
			      Result &out_result)
			:
				_root_fs(root_fs), _fd_path(fd_path), _handle(handle), _buf(buf),
				_count(count), _out_count(out_count), _out_result(out_result)
			{ }

			bool suspend() override
			{
				for (;;) {

					/* number of bytes written in one iteration */
					Vfs::file_size partial_out_count = 0;

					try {
						char const * const src = (char const *)_buf + _offset;

						_out_result = VFS_THREAD_SAFE(_handle->fs().write(_handle, src,
						                                                  _count, partial_out_count));
					} catch (Vfs::File_io_service::Insufficient_buffer) {
						retry = true;
						return retry;
					}

					if (_out_result != Result::WRITE_OK) {
						retry = false;
						return retry;
					}

					/* increment byte count reported to caller */
					_out_count += partial_out_count;

					bool const write_complete = (partial_out_count == _count);
					if (write_complete) {
						retry = false;
						return retry;
					}

					/*
					 * If the write has not consumed all bytes, set up
					 * another partial write iteration with the remaining
					 * bytes as 'count'.
					 *
					 * The costly 'fd_refers_to_continuous_file' is called
					 * for the first iteration only.
					 */
					bool const continuous_file = (_iteration > 0 || _fd_refers_to_continuous_file());

					if (!continuous_file) {
						warning("partial write on transactional file");
						_out_result = Result::WRITE_ERR_IO;
						retry = false;
						return retry;
					}

					_iteration++;

					bool const stalled = (partial_out_count == 0);
					if (stalled) {
						retry = true;
						return retry;
					}

					/* issue new write operation for remaining bytes */
					_count  -= partial_out_count;
					_offset += partial_out_count;
					_handle->advance_seek(partial_out_count);
				}
			}
		} check(_root_fs, fd->fd_path, handle, buf, count, out_count, out_result);

		do {
			suspend(check);
		} while (check.retry);

		/* XXX reset seek pointer after loop (will be advanced below by out_count) */
		handle->seek(initial_seek);
	}

	Plugin::resume_all();

	switch (out_result) {
	case Result::WRITE_ERR_AGAIN:       return Errno(EAGAIN);
	case Result::WRITE_ERR_WOULD_BLOCK: return Errno(EWOULDBLOCK);
	case Result::WRITE_ERR_INVALID:     return Errno(EINVAL);
	case Result::WRITE_ERR_IO:          return Errno(EIO);
	case Result::WRITE_ERR_INTERRUPT:   return Errno(EINTR);
	case Result::WRITE_OK:              break;
	}

	VFS_THREAD_SAFE(handle->advance_seek(out_count));
	fd->modified = true;

	return out_count;
}


ssize_t Libc::Vfs_plugin::read(File_descriptor *fd, void *buf,
                               ::size_t count)
{
	dispatch_pending_io_signals();

	if ((fd->flags & O_ACCMODE) == O_WRONLY) {
		return Errno(EBADF);
	}

	typedef Vfs::File_io_service::Read_result Result;

	Vfs::Vfs_handle *handle = vfs_handle(fd);

	if (fd->flags & O_DIRECTORY)
		return Errno(EISDIR);

	if (fd->flags & O_NONBLOCK && !read_ready(fd))
		return Errno(EAGAIN);

	{
		struct Check : Suspend_functor
		{
			bool             retry { false };

			Vfs::Vfs_handle *handle;
			::size_t         count;

			Check(Vfs::Vfs_handle *handle, ::size_t count)
			: handle(handle), count(count) { }

			bool suspend() override
			{
				retry = !VFS_THREAD_SAFE(handle->fs().queue_read(handle, count));
				return retry;
			}
		} check ( handle, count);

		do {
			suspend(check);
		} while (check.retry);
	}

	Vfs::file_size out_count = 0;
	Result         out_result;

	{
		struct Check : Suspend_functor
		{
			bool             retry { false };

			Vfs::Vfs_handle *handle;
			void            *buf;
			::size_t         count;
			Vfs::file_size  &out_count;
			Result          &out_result;

			Check(Vfs::Vfs_handle *handle, void *buf, ::size_t count,
			      Vfs::file_size &out_count, Result &out_result)
			: handle(handle), buf(buf), count(count), out_count(out_count),
			  out_result(out_result)
			{ }

			bool suspend() override
			{
				out_result = VFS_THREAD_SAFE(handle->fs().complete_read(handle, (char *)buf,
				                             count, out_count));
				/* suspend me if read is still queued */

				retry = (out_result == Result::READ_QUEUED);

				return retry;
			}
		} check ( handle, buf, count, out_count, out_result);

		do {
			suspend(check);
		} while (check.retry);
	}

	Plugin::resume_all();

	switch (out_result) {
	case Result::READ_ERR_AGAIN:       return Errno(EAGAIN);
	case Result::READ_ERR_WOULD_BLOCK: return Errno(EWOULDBLOCK);
	case Result::READ_ERR_INVALID:     return Errno(EINVAL);
	case Result::READ_ERR_IO:          return Errno(EIO);
	case Result::READ_ERR_INTERRUPT:   return Errno(EINTR);
	case Result::READ_OK:              break;

	case Result::READ_QUEUED: /* handled above, so never reached */ break;
	}

	VFS_THREAD_SAFE(handle->advance_seek(out_count));

	return out_count;
}


ssize_t Libc::Vfs_plugin::getdirentries(File_descriptor *fd, char *buf,
                                        ::size_t nbytes, ::off_t *basep)
{
	if (nbytes < sizeof(struct dirent)) {
		error("getdirentries: buffer too small");
		return -1;
	}

	typedef Vfs::File_io_service::Read_result Result;

	Vfs::Vfs_handle *handle = vfs_handle(fd);

	typedef Vfs::Directory_service::Dirent Dirent;

	Dirent dirent_out;

	{
		struct Check : Suspend_functor
		{
			bool             retry { false };

			Vfs::Vfs_handle *handle;

			Check(Vfs::Vfs_handle *handle)
			: handle(handle) { }

			bool suspend() override
			{
				retry = !VFS_THREAD_SAFE(handle->fs().queue_read(handle, sizeof(Dirent)));
				return retry;
			}
		} check(handle);

		do {
			suspend(check);
		} while (check.retry);
	}

	Result         out_result;
	Vfs::file_size out_count;

	{
		struct Check : Suspend_functor
		{
			bool             retry { false };

			Vfs::Vfs_handle *handle;
			Dirent          &dirent_out;
			Vfs::file_size  &out_count;
			Result          &out_result;

			Check(Vfs::Vfs_handle *handle, Dirent &dirent_out,
			      Vfs::file_size &out_count, Result &out_result)
			: handle(handle), dirent_out(dirent_out), out_count(out_count),
			  out_result(out_result) { }

			bool suspend() override
			{
				out_result = VFS_THREAD_SAFE(handle->fs().complete_read(handle,
				                             (char*)&dirent_out,
				                             sizeof(Dirent),
				                             out_count));
				dirent_out.sanitize();

				/* suspend me if read is still queued */

				retry = (out_result == Result::READ_QUEUED);

				return retry;
			}
		} check(handle, dirent_out, out_count, out_result);

		do {
			suspend(check);
		} while (check.retry);
	}

	Plugin::resume_all();

	if ((out_result != Result::READ_OK) ||
	    (out_count < sizeof(Dirent))) {
		return 0;
	}

	using Dirent_type = Vfs::Directory_service::Dirent_type;

	if (dirent_out.type == Dirent_type::END)
		return 0;

	/*
	 * Convert dirent structure from VFS to libc
	 */

	auto dirent_type = [] (Dirent_type type)
	{
		switch (type) {
		case Dirent_type::DIRECTORY:          return DT_DIR;
		case Dirent_type::CONTINUOUS_FILE:    return DT_REG;
		case Dirent_type::TRANSACTIONAL_FILE: return DT_SOCK;
		case Dirent_type::SYMLINK:            return DT_LNK;
		case Dirent_type::END:                return DT_UNKNOWN;
		}
		return DT_UNKNOWN;
	};

	dirent &dirent = *(struct dirent *)buf;
	dirent = { };

	dirent.d_type   = dirent_type(dirent_out.type);
	dirent.d_fileno = dirent_out.fileno;
	dirent.d_reclen = sizeof(struct dirent);

	Genode::strncpy(dirent.d_name, dirent_out.name.buf, sizeof(dirent.d_name));

	dirent.d_namlen = Genode::strlen(dirent.d_name);

	/*
	 * Keep track of VFS seek pointer and user-supplied basep.
	 */
	VFS_THREAD_SAFE(handle->advance_seek(sizeof(Vfs::Directory_service::Dirent)));

	*basep += sizeof(struct dirent);

	return sizeof(struct dirent);
}


int Libc::Vfs_plugin::ioctl(File_descriptor *fd, int request, char *argp)
{
	bool handled = false;

	if (request == TIOCGWINSZ) {

		if (!argp)
			return Errno(EINVAL);

		_with_info(*fd, [&] (Xml_node info) {
			if (info.type() == "terminal") {
				::winsize *winsize = (::winsize *)argp;
				winsize->ws_row = info.attribute_value("rows",    25U);
				winsize->ws_col = info.attribute_value("columns", 80U);
				handled = true;
			}
		});

	} else if (request == TIOCGETA) {

		::termios *termios = (::termios *)argp;

		termios->c_iflag = 0;
		termios->c_oflag = 0;
		termios->c_cflag = 0;
		/*
		 * Set 'ECHO' flag, needed by libreadline. Otherwise, echoing
		 * user input doesn't work in bash.
		 */
		termios->c_lflag = ECHO;
		::memset(termios->c_cc, _POSIX_VDISABLE, sizeof(termios->c_cc));
		termios->c_ispeed = 0;
		termios->c_ospeed = 0;
		handled = true;
	}

	if (handled)
		return 0;

	return _legacy_ioctl(fd, request, argp);
}


/**
 * Fallback for ioctl operations targeting the deprecated VFS ioctl interface
 *
 * XXX Remove this method once all ioctl operations are supported via
 *     regular VFS file accesses.
 */
int Libc::Vfs_plugin::_legacy_ioctl(File_descriptor *fd, int request, char *argp)
{
	using ::off_t;

	/*
	 * Marshal ioctl arguments
	 */

	typedef Vfs::File_io_service::Ioctl_opcode Opcode;

	Opcode opcode = Opcode::IOCTL_OP_UNDEFINED;

	Vfs::File_io_service::Ioctl_arg arg = 0;

	switch (request) {

	case TIOCGWINSZ:
		{
			if (!argp) {
				errno = EINVAL;
				return -1;
			}

			opcode = Opcode::IOCTL_OP_TIOCGWINSZ;
			break;
		}

	case TIOCSETAF:
		{
			opcode = Opcode::IOCTL_OP_TIOCSETAF;

			::termios *termios = (::termios *)argp;

			/*
			 * For now only enabling/disabling of ECHO is supported
			 */
			if (termios->c_lflag & (ECHO | ECHONL)) {
				arg = (Vfs::File_io_service::IOCTL_VAL_ECHO |
				       Vfs::File_io_service::IOCTL_VAL_ECHONL);
			}
			else {
				arg = Vfs::File_io_service::IOCTL_VAL_NULL;
			}

			break;
		}

	case TIOCSETAW:
		{
			opcode = Opcode::IOCTL_OP_TIOCSETAW;
			arg    = argp ? *(int*)argp : 0;
			break;
		}

	case FIONBIO:
		{
			opcode = Opcode::IOCTL_OP_FIONBIO;
			arg    = argp ? *(int*)argp : 0;
			break;
		}

	case DIOCGMEDIASIZE:
		{
			if (!argp) {
				errno = EINVAL;
				return -1;
			}

			opcode = Opcode::IOCTL_OP_DIOCGMEDIASIZE;
			arg    = 0;
			break;
		}

	default:
		warning("unsupported ioctl (request=", Hex(request), ")");
		break;
	}

	if (opcode == Opcode::IOCTL_OP_UNDEFINED) {
		errno = ENOTTY;
		return -1;
	}

	typedef Vfs::File_io_service::Ioctl_result Result;

	Vfs::File_io_service::Ioctl_out out;
	Genode::memset(&out, 0, sizeof(out));

	Vfs::Vfs_handle *handle = vfs_handle(fd);

	switch (VFS_THREAD_SAFE(handle->fs().ioctl(handle, opcode, arg, out))) {
	case Result::IOCTL_ERR_INVALID: errno = EINVAL; return -1;
	case Result::IOCTL_ERR_NOTTY:   errno = ENOTTY; return -1;
	case Result::IOCTL_OK:                          break;
	}

	/*
	 * Unmarshal ioctl results
	 */
	switch (request) {

	case TIOCGWINSZ:
		{
			::winsize *winsize = (::winsize *)argp;
			winsize->ws_row = out.tiocgwinsz.rows;
			winsize->ws_col = out.tiocgwinsz.columns;
			return 0;
		}
	case TIOCSETAF:
	case TIOCSETAW:
		return 0;

	case FIONBIO:
		return 0;

	case DIOCGMEDIASIZE:
		{
			/* resolve ambiguity with libc type */
			using Genode::int64_t;

			int64_t *disk_size = (int64_t*)argp;
			*disk_size = out.diocgmediasize.size;
			return 0;
		}

	default:
		break;
	}

	return -1;
}


::off_t Libc::Vfs_plugin::lseek(File_descriptor *fd, ::off_t offset, int whence)
{
	Vfs::Vfs_handle *handle = vfs_handle(fd);

	switch (whence) {
	case SEEK_SET: handle->seek(offset); break;
	case SEEK_CUR: handle->advance_seek(offset); break;
	case SEEK_END:
		{
			struct stat stat;
			::memset(&stat, 0, sizeof(stat));
			fstat(fd, &stat);
			handle->seek(stat.st_size + offset);
		}
		break;
	}
	return handle->seek();
}


int Libc::Vfs_plugin::ftruncate(File_descriptor *fd, ::off_t length)
{
	Vfs::Vfs_handle *handle = vfs_handle(fd);
	if (fd->modified) {
		_vfs_sync(*handle);
		fd->modified = false;
	}

	typedef Vfs::File_io_service::Ftruncate_result Result;

	switch (VFS_THREAD_SAFE(handle->fs().ftruncate(handle, length))) {
	case Result::FTRUNCATE_ERR_NO_PERM:   errno = EPERM;  return -1;
	case Result::FTRUNCATE_ERR_INTERRUPT: errno = EINTR;  return -1;
	case Result::FTRUNCATE_ERR_NO_SPACE:  errno = ENOSPC; return -1;
	case Result::FTRUNCATE_OK:                            break;
	}
	return 0;
}


int Libc::Vfs_plugin::fcntl(File_descriptor *fd, int cmd, long arg)
{
	switch (cmd) {
	case F_DUPFD_CLOEXEC:
	case F_DUPFD:
		{
			/*
			 * Allocate free file descriptor locally.
			 */
			File_descriptor *new_fd =
				file_descriptor_allocator()->alloc(this, 0);
			if (!new_fd) return Errno(EMFILE);

			/*
			 * Use new allocated number as name of file descriptor
			 * duplicate.
			 */
			if (Vfs_plugin::dup2(fd, new_fd) == -1) {
				error("Plugin::fcntl: dup2 unexpectedly failed");
				return Errno(EINVAL);
			}

			return new_fd->libc_fd;
		}
	case F_GETFD: return fd->cloexec ? FD_CLOEXEC : 0;
	case F_SETFD: fd->cloexec = arg == FD_CLOEXEC;  return 0;

	case F_GETFL: return fd->flags;
	case F_SETFL: {
			/* only the specified flags may be changed */
			long const mask = (O_NONBLOCK | O_APPEND | O_ASYNC | O_FSYNC);
			fd->flags = (fd->flags & ~mask) | (arg & mask);
		} return 0;

	default:
		break;
	}

	error("fcntl(): command ", cmd, " not supported - vfs");
	return Errno(EINVAL);
}


int Libc::Vfs_plugin::fsync(File_descriptor *fd)
{
	Vfs::Vfs_handle *handle = vfs_handle(fd);
	if (!fd->modified) { return 0; }

	/*
	 * XXX checking the return value of _vfs_sync amd returning -1
	 * in the EIO case will break the lighttpd or rather the socket_fs
	 */
	fd->modified = !!_vfs_sync(*handle);

	return 0;
}


int Libc::Vfs_plugin::symlink(const char *oldpath, const char *newpath)
{
	typedef Vfs::Directory_service::Openlink_result Openlink_result;

	Vfs::Vfs_handle *handle { 0 };

	Openlink_result openlink_result =
		VFS_THREAD_SAFE(_root_fs.openlink(newpath, true, &handle, _alloc));

	switch (openlink_result) {
	case Openlink_result::OPENLINK_OK:
		break;
	case Openlink_result::OPENLINK_ERR_LOOKUP_FAILED:
		return Errno(ENOENT);
	case Openlink_result::OPENLINK_ERR_NAME_TOO_LONG:
		return Errno(ENAMETOOLONG);
	case Openlink_result::OPENLINK_ERR_NODE_ALREADY_EXISTS:
		return Errno(EEXIST);
	case Openlink_result::OPENLINK_ERR_NO_SPACE:
		return Errno(ENOSPC);
	case Openlink_result::OPENLINK_ERR_OUT_OF_RAM:
		return Errno(ENOSPC);
	case Openlink_result::OPENLINK_ERR_OUT_OF_CAPS:
		return Errno(ENOSPC);
	case Vfs::Directory_service::OPENLINK_ERR_PERMISSION_DENIED:
		return Errno(EPERM);
	}

	Vfs::file_size count = ::strlen(oldpath) + 1;
	Vfs::file_size out_count  = 0;

	handle->handler(&_response_handler);

	struct Check : Suspend_functor
	{
		bool             retry { false };

		Vfs::Vfs_handle *handle;
		void const      *buf;
		::size_t         count;
		Vfs::file_size  &out_count;

		Check(Vfs::Vfs_handle *handle, void const *buf,
		      ::size_t count, Vfs::file_size &out_count)
		: handle(handle), buf(buf), count(count), out_count(out_count)
		{ }

		bool suspend() override
		{
			try {
				VFS_THREAD_SAFE(handle->fs().write(handle, (char const *)buf,
					              count, out_count));
				retry = false;
			} catch (Vfs::File_io_service::Insufficient_buffer) {
				retry = true;
			}

			return retry;
		}
	} check ( handle, oldpath, count, out_count);

	do {
		suspend(check);
	} while (check.retry);

	Plugin::resume_all();

	_vfs_sync(*handle);
	VFS_THREAD_SAFE(handle->close());

	if (out_count != count)
		return Errno(ENAMETOOLONG);

	return 0;
}


ssize_t Libc::Vfs_plugin::readlink(const char *path, char *buf, ::size_t buf_size)
{
	Vfs::Vfs_handle *symlink_handle { 0 };

	Vfs::Directory_service::Openlink_result openlink_result =
		VFS_THREAD_SAFE(_root_fs.openlink(path, false, &symlink_handle, _alloc));

	switch (openlink_result) {
	case Vfs::Directory_service::OPENLINK_OK:
		break;
	case Vfs::Directory_service::OPENLINK_ERR_LOOKUP_FAILED:
		return Errno(ENOENT);
	case Vfs::Directory_service::OPENLINK_ERR_NAME_TOO_LONG:
		/* should not happen */
		return Errno(ENAMETOOLONG);
	case Vfs::Directory_service::OPENLINK_ERR_NODE_ALREADY_EXISTS:
	case Vfs::Directory_service::OPENLINK_ERR_NO_SPACE:
	case Vfs::Directory_service::OPENLINK_ERR_OUT_OF_RAM:
	case Vfs::Directory_service::OPENLINK_ERR_OUT_OF_CAPS:
	case Vfs::Directory_service::OPENLINK_ERR_PERMISSION_DENIED:
		return Errno(EACCES);
	}

	symlink_handle->handler(&_response_handler);

	{
		struct Check : Suspend_functor
		{
			bool             retry { false };

			Vfs::Vfs_handle *symlink_handle;
			::size_t const   buf_size;

			Check(Vfs::Vfs_handle *symlink_handle,
			      ::size_t const buf_size)
			: symlink_handle(symlink_handle), buf_size(buf_size) { }

			bool suspend() override
			{
				retry =
					!VFS_THREAD_SAFE(symlink_handle->fs().queue_read(symlink_handle, buf_size));
				return retry;
			}
		} check(symlink_handle, buf_size);

		do {
			suspend(check);
		} while (check.retry);
	}

	typedef Vfs::File_io_service::Read_result Result;

	Result out_result;
	Vfs::file_size out_len = 0;

	{
		struct Check : Suspend_functor
		{
			bool             retry { false };

			Vfs::Vfs_handle *symlink_handle;
			char            *buf;
			::size_t const   buf_size;
			Vfs::file_size  &out_len;
			Result          &out_result;

			Check(Vfs::Vfs_handle *symlink_handle,
			      char *buf,
			      ::size_t const buf_size,
			      Vfs::file_size &out_len,
			      Result &out_result)
			: symlink_handle(symlink_handle), buf(buf), buf_size(buf_size),
			  out_len(out_len), out_result(out_result) { }

			bool suspend() override
			{
				out_result = VFS_THREAD_SAFE(symlink_handle->fs().complete_read(symlink_handle, buf, buf_size, out_len));

				/* suspend me if read is still queued */

				retry = (out_result == Result::READ_QUEUED);

				return retry;
			}
		} check(symlink_handle, buf, buf_size, out_len, out_result);

		do {
			suspend(check);
		} while (check.retry);
	}

	Plugin::resume_all();

	switch (out_result) {
	case Result::READ_ERR_AGAIN:       return Errno(EAGAIN);
	case Result::READ_ERR_WOULD_BLOCK: return Errno(EWOULDBLOCK);
	case Result::READ_ERR_INVALID:     return Errno(EINVAL);
	case Result::READ_ERR_IO:          return Errno(EIO);
	case Result::READ_ERR_INTERRUPT:   return Errno(EINTR);
	case Result::READ_OK:              break;

	case Result::READ_QUEUED: /* handled above, so never reached */ break;
	};

	VFS_THREAD_SAFE(symlink_handle->close());

	return out_len;
}


int Libc::Vfs_plugin::rmdir(char const *path)
{
	return unlink(path);
}


int Libc::Vfs_plugin::unlink(char const *path)
{
	typedef Vfs::Directory_service::Unlink_result Result;

	switch (VFS_THREAD_SAFE(_root_fs.unlink(path))) {
	case Result::UNLINK_ERR_NO_ENTRY:  errno = ENOENT;    return -1;
	case Result::UNLINK_ERR_NO_PERM:   errno = EPERM;     return -1;
	case Result::UNLINK_ERR_NOT_EMPTY: errno = ENOTEMPTY; return -1;
	case Result::UNLINK_OK:            break;
	}
	return 0;
}


int Libc::Vfs_plugin::rename(char const *from_path, char const *to_path)
{
	typedef Vfs::Directory_service::Rename_result Result;

	if (VFS_THREAD_SAFE(_root_fs.leaf_path(to_path))) {

		if (VFS_THREAD_SAFE(_root_fs.directory(to_path))) {
			if (!VFS_THREAD_SAFE(_root_fs.directory(from_path))) {
				errno = EISDIR; return -1;
			}

			if (VFS_THREAD_SAFE(_root_fs.num_dirent(to_path))) {
				errno = ENOTEMPTY; return -1;
			}

		} else {
			if (VFS_THREAD_SAFE(_root_fs.directory(from_path))) {
				errno = ENOTDIR; return -1;
			}
		}
	}

	switch (VFS_THREAD_SAFE(_root_fs.rename(from_path, to_path))) {
	case Result::RENAME_ERR_NO_ENTRY: errno = ENOENT; return -1;
	case Result::RENAME_ERR_CROSS_FS: errno = EXDEV;  return -1;
	case Result::RENAME_ERR_NO_PERM:  errno = EPERM;  return -1;
	case Result::RENAME_OK:                           break;
	}
	return 0;
}


void *Libc::Vfs_plugin::mmap(void *addr_in, ::size_t length, int prot, int flags,
                             File_descriptor *fd, ::off_t offset)
{
	if (prot != PROT_READ && !(prot == (PROT_READ | PROT_WRITE) && flags == MAP_PRIVATE)) {
		error("mmap for prot=", Hex(prot), " not supported");
		errno = EACCES;
		return (void *)-1;
	}

	if (addr_in != 0) {
		error("mmap for predefined address not supported");
		errno = EINVAL;
		return (void *)-1;
	}

	/*
	 * XXX attempt to obtain memory mapping via
	 *     'Vfs::Directory_service::dataspace'.
	 */

	void *addr = mem_alloc()->alloc(length, PAGE_SHIFT);
	if (addr == (void *)-1) {
		errno = ENOMEM;
		return (void *)-1;
	}

	/* copy variables for complete read */
	size_t read_remain = length;
	size_t read_offset = offset;
	char *read_addr = (char *)addr;

	while (read_remain > 0) {
		ssize_t length_read = ::pread(fd->libc_fd, read_addr, read_remain, read_offset);
		if (length_read < 0) { /* error */
			error("mmap could not obtain file content");
			::munmap(addr, length);
			errno = EACCES;
			return (void *)-1;
		} else if (length_read == 0) /* EOF */
			break; /* done (length can legally be greater than the file length) */
		read_remain -= length_read;
		read_offset += length_read;
		read_addr += length_read;
	}

	return addr;
}


int Libc::Vfs_plugin::munmap(void *addr, ::size_t)
{
	mem_alloc()->free(addr);
	return 0;
}


int Libc::Vfs_plugin::pipe(Libc::File_descriptor *pipefdo[2])
{
	Absolute_path base_path(Libc::config_pipe());
	if (base_path == "") {
		error(__func__, ": pipe fs not mounted");
		return Errno(EACCES);
	}

	Libc::File_descriptor *meta_fd { nullptr };

	{
		Absolute_path new_path = base_path;
		new_path.append("/new");

		meta_fd = open(new_path.base(), O_RDONLY, Libc::ANY_FD);
		if (!meta_fd) {
			Genode::error("failed to create pipe at ", new_path);
			return Errno(EACCES);
		}
		meta_fd->path(new_path.string());

		char buf[32] { };
		int const n = read(meta_fd, buf, sizeof(buf)-1);
		if (n < 1) {
			error("failed to read pipe at ", new_path);
			close(meta_fd);
			return Errno(EACCES);
		}
		buf[n] = '\0';
		base_path.append("/");
		base_path.append(buf);
	}

	auto open_pipe_fd = [&] (auto path_suffix, auto flags)
	{
		Absolute_path path = base_path;
		path.append(path_suffix);

		File_descriptor *fd = open(path.base(), flags, Libc::ANY_FD);
		if (!fd)
			error("failed to open pipe end at ", path);
		else
			fd->path(path.string());

		return fd;
	};

	pipefdo[0] = open_pipe_fd("/out", O_RDONLY);
	pipefdo[1] = open_pipe_fd("/in",  O_WRONLY);

	close(meta_fd);

	if (!pipefdo[0] || !pipefdo[1])
		return Errno(EACCES);

	return 0;
}


bool Libc::Vfs_plugin::poll(File_descriptor &fd, struct pollfd &pfd)
{
	if (fd.plugin != this) return false;

	Vfs::Vfs_handle *handle = vfs_handle(&fd);
	if (!handle) {
		pfd.revents |= POLLNVAL;
		return true;
	}

	enum {
		POLLIN_MASK = POLLIN | POLLRDNORM | POLLRDBAND | POLLPRI,
		POLLOUT_MASK = POLLOUT | POLLWRNORM | POLLWRBAND,
	};

	bool res { false };

	if ((pfd.events & POLLIN_MASK)
	 && VFS_THREAD_SAFE(handle->fs().read_ready(handle)))
	{
		pfd.revents |= pfd.events & POLLIN_MASK;
		res = true;
	}

	if ((pfd.events & POLLOUT_MASK) /* XXX always writeable */)
	{
		pfd.revents |= pfd.events & POLLOUT_MASK;
		res = true;
	}

	return res;
}


bool Libc::Vfs_plugin::supports_select(int nfds,
                                       fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
                                       struct timeval *timeout)
{
	/* return true if any file descriptor (which is set) belongs to the VFS */
	for (int fd = 0; fd < nfds; ++fd) {

		if (FD_ISSET(fd, readfds) || FD_ISSET(fd, writefds) || FD_ISSET(fd, exceptfds)) {

			File_descriptor *fdo =
				file_descriptor_allocator()->find_by_libc_fd(fd);

			if (fdo && (fdo->plugin == this))
				return true;
		}
	}
	return false;
}


int Libc::Vfs_plugin::select(int nfds,
                             fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
                             struct timeval *timeout)
{
	int nready = 0;

	fd_set const in_readfds  = *readfds;
	fd_set const in_writefds = *writefds;
	/* XXX exceptfds not supported */

	/* clear fd sets */
	FD_ZERO(readfds);
	FD_ZERO(writefds);
	FD_ZERO(exceptfds);

	for (int fd = 0; fd < nfds; ++fd) {

		File_descriptor *fdo =
			file_descriptor_allocator()->find_by_libc_fd(fd);

		/* handle only fds that belong to this plugin */
		if (!fdo || (fdo->plugin != this))
			continue;

		Vfs::Vfs_handle *handle = vfs_handle(fdo);
		if (!handle) continue;

		if (FD_ISSET(fd, &in_readfds)) {
			if (VFS_THREAD_SAFE(handle->fs().read_ready(handle))) {
				FD_SET(fd, readfds);
				++nready;
			} else {
				notify_read_ready(handle);
			}
		}

		if (FD_ISSET(fd, &in_writefds)) {
			if (true /* XXX always writeable */) {
				FD_SET(fd, writefds);
				++nready;
			}
		}

		/* XXX exceptfds not supported */
	}
	return nready;
}
