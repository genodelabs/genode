/*
 * \brief  Libc plugin for using a process-local virtual file system
 * \author Norman Feske
 * \author Christian Helmuth
 * \author Emery Hemingway
 * \date   2014-04-09
 */

/*
 * Copyright (C) 2014-2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/env.h>
#include <base/log.h>
#include <vfs/dir_file_system.h>
#include <net/mac_address.h>

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
#include <sys/soundcard.h>
#include <dlfcn.h>
#include <net/if.h>
#include <net/if_tap.h>

/* libc-internal includes */
#include <internal/plugin.h>
#include <internal/kernel.h>
#include <internal/vfs_plugin.h>
#include <internal/mem_alloc.h>
#include <internal/errno.h>
#include <internal/init.h>
#include <internal/monitor.h>
#include <internal/current_time.h>


static Libc::Monitor      *_monitor_ptr;
static Genode::Region_map *_region_map_ptr;


void Libc::init_vfs_plugin(Monitor &monitor, Genode::Region_map &rm)
{
	_monitor_ptr = &monitor;
	_region_map_ptr = &rm;
}


static Libc::Monitor & monitor()
{
	struct Missing_call_of_init_vfs_plugin : Genode::Exception { };
	if (!_monitor_ptr)
		throw Missing_call_of_init_vfs_plugin();
	return *_monitor_ptr;
}


static Genode::Region_map &region_map()
{
	struct Missing_call_of_init_vfs_plugin : Genode::Exception { };
	if (!_region_map_ptr)
		throw Missing_call_of_init_vfs_plugin();
	return *_region_map_ptr;
}


namespace { using Fn = Libc::Monitor::Function_result; }


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
	enum { FS_BLOCK_SIZE = 4096 * 16 };

	unsigned const readable_bits   = S_IRUSR,
	               writeable_bits  = S_IWUSR,
	               executable_bits = S_IXUSR;

	auto type = [] (Vfs::Node_type type)
	{
		switch (type) {
		case Vfs::Node_type::DIRECTORY:          return S_IFDIR;
		case Vfs::Node_type::CONTINUOUS_FILE:    return S_IFREG;
		case Vfs::Node_type::TRANSACTIONAL_FILE: return S_IFCHR;
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
		static Genode::String<Vfs::MAX_PATH_LEN> default_value {
			config_socket(), "/nameserver" };

		static Config_attr ns_file("nameserver_file",
		                           default_value.string());
		return ns_file.string();
	}

	void libc_config_init(Xml_node node)
	{
		static Xml_node config = node;
		_config_node = &config;

		libc_resolv_path = config_nameserver_file();
	}

	bool read_ready_from_kernel(File_descriptor *fd)
	{
		Vfs::Vfs_handle *handle = vfs_handle(fd);
		if (!handle) return false;

		handle->fs().notify_read_ready(handle);

		return handle->fs().read_ready(*handle);
	}

	bool write_ready_from_kernel(File_descriptor *fd)
	{
		Vfs::Vfs_handle const *handle = vfs_handle(fd);
		if (!handle)
			return false;

		return handle->fs().write_ready(*handle);
	}
}


/*
 * This function must be called in entrypoint context only.
 */
template <typename FN>
void Libc::Vfs_plugin::_with_info(File_descriptor &fd, FN const &fn)
{
	Absolute_path path = ioctl_dir(fd);
	path.append_element("info");

	_cached_ioctl_info.with_file(path, [&] (Readonly_file const &file) {

		char buffer[4096] { };

		Byte_range_ptr range(buffer,
		                     min((size_t)(_root_dir->file_size(path.string())),
		                         sizeof(buffer)));

		with_xml_file_content(file, range, [&] (Xml_node node) { fn(node); });
	});
}


int Libc::Vfs_plugin::access(const char *path, int amode)
{
	bool succeeded = false;
	monitor().monitor([&] {
		if (_root_fs.leaf_path(path))
			succeeded = true;
		return Fn::COMPLETE;
	});
	if (succeeded)
		return 0;

	errno = ENOENT;
	return -1;
}


Libc::File_descriptor *Libc::Vfs_plugin::open_from_kernel(const char *path, int flags, int libc_fd)
{
	if (_root_fs.directory(path)) {

		if (((flags & O_ACCMODE) != O_RDONLY)) {
			errno = EISDIR;
			return nullptr;
		}

		flags |= O_DIRECTORY;

		Vfs::Vfs_handle *handle = 0;

		typedef Vfs::Directory_service::Opendir_result Opendir_result;

		switch (_root_fs.opendir(path, false, &handle, _alloc)) {
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

		if (!fd) {
			handle->close();
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

		switch (_root_fs.open(path, flags, &handle, _alloc)) {

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
				switch (_root_fs.open(path, flags | O_EXCL, &handle, _alloc)) {

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

	if (!fd) {
		handle->close();
		errno = EMFILE;
		return nullptr;
	}

	handle->handler(&_response_handler);
	fd->flags = flags & (O_ACCMODE|O_NONBLOCK|O_APPEND);

	if (flags & O_TRUNC)
		warning(__func__, ": O_TRUNC is not supported");

	return fd;
}


Libc::File_descriptor *Libc::Vfs_plugin::open(char const *path, int flags)
{
	File_descriptor *fd = nullptr;
	int result_errno = 0;
	{
		monitor().monitor([&] {

			/* handle open for directories */
			if (_root_fs.directory(path)) {

				if (((flags & O_ACCMODE) != O_RDONLY)) {
					result_errno = EISDIR;
					return Fn::COMPLETE;
				}

				flags |= O_DIRECTORY;

				Vfs::Vfs_handle *handle = 0;

				typedef Vfs::Directory_service::Opendir_result Opendir_result;

				switch (_root_fs.opendir(path, false, &handle, _alloc)) {
				case Opendir_result::OPENDIR_OK:                      break;
				case Opendir_result::OPENDIR_ERR_LOOKUP_FAILED:       result_errno = ENOENT;       return Fn::COMPLETE;
				case Opendir_result::OPENDIR_ERR_NAME_TOO_LONG:       result_errno = ENAMETOOLONG; return Fn::COMPLETE;
				case Opendir_result::OPENDIR_ERR_NODE_ALREADY_EXISTS: result_errno = EEXIST;       return Fn::COMPLETE;
				case Opendir_result::OPENDIR_ERR_NO_SPACE:            result_errno = ENOSPC;       return Fn::COMPLETE;
				case Opendir_result::OPENDIR_ERR_OUT_OF_RAM:
				case Opendir_result::OPENDIR_ERR_OUT_OF_CAPS:
				case Opendir_result::OPENDIR_ERR_PERMISSION_DENIED:   result_errno = EPERM;        return Fn::COMPLETE;
				}

				/* the directory was successfully opened */

				fd = file_descriptor_allocator()->alloc(this, vfs_context(handle), Libc::ANY_FD);

				if (!fd) {
					handle->close();
					result_errno = EMFILE;
					return Fn::COMPLETE;
				}

				handle->handler(&_response_handler);
				fd->flags = flags & O_ACCMODE;

				return Fn::COMPLETE;
			}

			if (flags & O_DIRECTORY) {
				result_errno = ENOTDIR;
				return Fn::COMPLETE;
			}

			/* handle open for files */
			typedef Vfs::Directory_service::Open_result Result;

			Vfs::Vfs_handle *handle = 0;

			while (handle == nullptr) {

				switch (_root_fs.open(path, flags, &handle, _alloc)) {

				case Result::OPEN_OK:
					break;

				case Result::OPEN_ERR_UNACCESSIBLE:
					{
						if (!(flags & O_CREAT)) {
							if (flags & O_NOFOLLOW) {
							        result_errno = ELOOP;
							        return Fn::COMPLETE;
							}
							result_errno = ENOENT;
							return Fn::COMPLETE;
						}

						/* O_CREAT is set, so try to create the file */
						switch (_root_fs.open(path, flags | O_EXCL, &handle, _alloc)) {

						case Result::OPEN_OK:
							break;

						case Result::OPEN_ERR_EXISTS:

							/* file has been created by someone else in the meantime */
							if (flags & O_NOFOLLOW) {
								result_errno = ELOOP;
								return Fn::COMPLETE;
							}
							result_errno = EEXIST;
							return Fn::COMPLETE;

						case Result::OPEN_ERR_NO_PERM:       result_errno = EPERM;        return Fn::COMPLETE;
						case Result::OPEN_ERR_UNACCESSIBLE:  result_errno = ENOENT;       return Fn::COMPLETE;
						case Result::OPEN_ERR_NAME_TOO_LONG: result_errno = ENAMETOOLONG; return Fn::COMPLETE;
						case Result::OPEN_ERR_NO_SPACE:      result_errno = ENOSPC;       return Fn::COMPLETE;
						case Result::OPEN_ERR_OUT_OF_RAM:    result_errno = ENOSPC;       return Fn::COMPLETE;
						case Result::OPEN_ERR_OUT_OF_CAPS:   result_errno = ENOSPC;       return Fn::COMPLETE;
						}
					}
					break;

				case Result::OPEN_ERR_NO_PERM:       result_errno = EPERM;        return Fn::COMPLETE;
				case Result::OPEN_ERR_EXISTS:        result_errno = EEXIST;       return Fn::COMPLETE;
				case Result::OPEN_ERR_NAME_TOO_LONG: result_errno = ENAMETOOLONG; return Fn::COMPLETE;
				case Result::OPEN_ERR_NO_SPACE:      result_errno = ENOSPC;       return Fn::COMPLETE;
				case Result::OPEN_ERR_OUT_OF_RAM:    result_errno = ENOSPC;       return Fn::COMPLETE;
				case Result::OPEN_ERR_OUT_OF_CAPS:   result_errno = ENOSPC;       return Fn::COMPLETE;
				}
			}

			/* the file was successfully opened */

			fd = file_descriptor_allocator()->alloc(this, vfs_context(handle), Libc::ANY_FD);

			if (!fd) {
				handle->close();
				result_errno = EMFILE;
				return Fn::COMPLETE;
			}

			handle->handler(&_response_handler);
			fd->flags = flags & (O_ACCMODE|O_NONBLOCK|O_APPEND);

			return Fn::COMPLETE;
		});
	}

	if (!fd)
		errno = result_errno;

	if (fd && (flags & O_TRUNC) && (ftruncate(fd, 0) == -1)) {
		vfs_handle(fd)->close();
		errno = EINVAL; /* XXX which error code fits best ? */
		fd = nullptr;
	}

	return fd;
}


struct Sync
{
	enum { INITIAL, TIMESTAMP_UPDATED, QUEUED, COMPLETE } state { INITIAL };

	Vfs::Vfs_handle &vfs_handle;
	Vfs::Timestamp   mtime { Vfs::Timestamp::INVALID };

	Sync(Vfs::Vfs_handle &vfs_handle, Libc::Vfs_plugin::Update_mtime update_mtime,
	     Libc::Current_real_time &current_real_time)
	:
		vfs_handle(vfs_handle)
	{
		if (update_mtime == Libc::Vfs_plugin::Update_mtime::NO
		 || !current_real_time.has_real_time()) {

			state = TIMESTAMP_UPDATED;

		} else {
			timespec const ts = current_real_time.current_real_time();

			mtime = { .value = (long long)ts.tv_sec };
		}
	}

	bool complete()
	{
		switch (state) {
		case Sync::INITIAL:
			if (!vfs_handle.fs().update_modification_timestamp(&vfs_handle, mtime))
				return false;
			state = Sync::TIMESTAMP_UPDATED; [[ fallthrough ]];
		case Sync::TIMESTAMP_UPDATED:
			if (!vfs_handle.fs().queue_sync(&vfs_handle))
				return false;
			state = Sync::QUEUED; [[ fallthrough ]];
		case Sync::QUEUED:
			if (vfs_handle.fs().complete_sync(&vfs_handle) == Vfs::File_io_service::SYNC_QUEUED)
				return false;
			state = Sync::COMPLETE; [[ fallthrough ]];
		case Sync::COMPLETE:
			break;
		}
		return true;
	}
};


int Libc::Vfs_plugin::close_from_kernel(File_descriptor *fd)
{
	Vfs::Vfs_handle *handle = vfs_handle(fd);

	if ((fd->modified) || (fd->flags & O_CREAT)) {
		/* XXX mtime not updated here */
		Sync sync { *handle, Update_mtime::NO, _current_real_time };

		while (!sync.complete()) {
			Libc::Kernel::kernel().wakeup_remote_peers();
			Libc::Kernel::kernel().libc_env().ep().wait_and_dispatch_one_io_signal();
		}
	}

	handle->close();
	file_descriptor_allocator()->free(fd);

	return 0;
}


int Libc::Vfs_plugin::close(File_descriptor *fd)
{
	Vfs::Vfs_handle *handle = vfs_handle(fd);

	Sync sync { *handle , _update_mtime, _current_real_time };

	monitor().monitor([&] {
		if ((fd->modified) || (fd->flags & O_CREAT))
			if (!sync.complete())
				return Fn::INCOMPLETE;

		handle->close();
		file_descriptor_allocator()->free(fd);

		return Fn::COMPLETE;
	});

	return 0;
}


int Libc::Vfs_plugin::dup2(File_descriptor *fd,
                           File_descriptor *new_fd)
{
	Vfs::Vfs_handle *handle = nullptr;

	typedef Vfs::Directory_service::Open_result Result;

	int result = -1;
	monitor().monitor([&] {
		if (_root_fs.open(fd->fd_path, fd->flags, &handle, _alloc) != Result::OPEN_OK) {

			warning("dup2 failed for path ", fd->fd_path);
			result = Errno(EBADF);
			return Fn::COMPLETE;
		}

		handle->seek(vfs_handle(fd)->seek());
		handle->handler(&_response_handler);

		new_fd->context = vfs_context(handle);
		new_fd->flags = fd->flags;
		new_fd->path(fd->fd_path);

		result = new_fd->libc_fd;
		return Fn::COMPLETE;
	});
	return result;
}


Libc::File_descriptor *Libc::Vfs_plugin::dup(File_descriptor *fd)
{
	Vfs::Vfs_handle *handle = nullptr;

	typedef Vfs::Directory_service::Open_result Result;

	Libc::File_descriptor *result = nullptr;
	int result_errno = 0;
	monitor().monitor([&] {
		if (_root_fs.open(fd->fd_path, fd->flags, &handle, _alloc) != Result::OPEN_OK) {

			warning("dup failed for path ", fd->fd_path);
			result_errno = EBADF;
			return Fn::COMPLETE;
		}

		handle->seek(vfs_handle(fd)->seek());
		handle->handler(&_response_handler);

		File_descriptor * const new_fd =
			file_descriptor_allocator()->alloc(this, vfs_context(handle));

		if (!new_fd) {
			handle->close();
			result_errno = EMFILE;
			return Fn::COMPLETE;
		}

		new_fd->flags = fd->flags;
		new_fd->path(fd->fd_path);

		result = new_fd;
		return Fn::COMPLETE;
	});

	if (!result)
		errno = result_errno;

	return result;
}


int Libc::Vfs_plugin::fstat(File_descriptor *fd, struct stat *buf)
{
	Vfs::Vfs_handle *handle = vfs_handle(fd);

	if (fd->modified) {
		Sync sync { *handle , _update_mtime, _current_real_time };

		monitor().monitor([&] {
			if (!sync.complete()) {
				return Fn::INCOMPLETE;
			}
			return Fn::COMPLETE;
		});
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

	int result = -1;
	int result_errno = 0;
	monitor().monitor([&] {
		switch (_root_fs.opendir(path, true, &dir_handle, _alloc)) {
		case Opendir_result::OPENDIR_ERR_LOOKUP_FAILED:       result_errno = ENOENT;       break;
		case Opendir_result::OPENDIR_ERR_NAME_TOO_LONG:       result_errno = ENAMETOOLONG; break;
		case Opendir_result::OPENDIR_ERR_NODE_ALREADY_EXISTS: result_errno = EEXIST;       break;
		case Opendir_result::OPENDIR_ERR_NO_SPACE:            result_errno = ENOSPC;       break;
		case Opendir_result::OPENDIR_ERR_OUT_OF_RAM:          result_errno = EPERM;        break;
		case Opendir_result::OPENDIR_ERR_OUT_OF_CAPS:         result_errno = EPERM;        break;
		case Opendir_result::OPENDIR_ERR_PERMISSION_DENIED:   result_errno = EPERM;        break;
		case Opendir_result::OPENDIR_OK:
			dir_handle->close();
			result = 0;
			break;
		}

		return Fn::COMPLETE;
	});

	if (result == -1)
		errno = result_errno;

	return result;
}


int Libc::Vfs_plugin::stat_from_kernel(const char *path, struct stat *buf)
{
	if (!path or !buf)
		return Errno(EFAULT);


	typedef Vfs::Directory_service::Stat_result Result;

	Vfs::Directory_service::Stat stat;

	switch (_root_fs.stat(path, stat)) {
	case Result::STAT_ERR_NO_ENTRY: errno = ENOENT; return -1;
	case Result::STAT_ERR_NO_PERM:  errno = EACCES; return -1;
	case Result::STAT_OK:                           break;
	}

	vfs_stat_to_libc_stat_struct(stat, buf);
	return 0;
}


int Libc::Vfs_plugin::stat(char const *path, struct stat *buf)
{
	if (!path or !buf) {
		return Errno(EFAULT);
	}

	typedef Vfs::Directory_service::Stat_result Result;

	Vfs::Directory_service::Stat stat;

	int result = -1;
	int result_errno = 0;
	monitor().monitor([&] {
		switch (_root_fs.stat(path, stat)) {
		case Result::STAT_ERR_NO_ENTRY: result_errno = ENOENT; break;
		case Result::STAT_ERR_NO_PERM:  result_errno = EACCES; break;
		case Result::STAT_OK:
			vfs_stat_to_libc_stat_struct(stat, buf);
			result = 0;
			break;
		}

		return Fn::COMPLETE;
	});

	if (result == -1)
		errno = result_errno;

	return result;
}


ssize_t Libc::Vfs_plugin::write(File_descriptor *fd, const void *buf,
                                ::size_t count)
{
	typedef Vfs::File_io_service::Write_result Result;

	if ((fd->flags & O_ACCMODE) == O_RDONLY) {
		return Errno(EBADF);
	}

	Vfs::Vfs_handle *handle = vfs_handle(fd);

	::size_t out_count  = 0;
	Result   out_result = Result::WRITE_OK;

	Const_byte_range_ptr const src { (char const *)buf, count };

	if (fd->flags & O_NONBLOCK) {
		monitor().monitor([&] {
			out_result = handle->fs().write(handle, src, out_count);
			return Fn::COMPLETE;
		});
	} else {
		Vfs::file_size const initial_seek { handle->seek() };

		/* TODO clean this up */
		char const * const _fd_path    { fd->fd_path };
		Vfs::Vfs_handle   *_handle     { handle };
		void const        *_buf        { buf };
		::size_t           _count      { count };
		::size_t          &_out_count  { out_count };
		Result            &_out_result { out_result };
		::off_t            _offset     { 0 };
		unsigned           _iteration  { 0 };

		auto _fd_refers_to_continuous_file = [&]
		{
			if (!_fd_path) {
				warning("Vfs_plugin: _fd_refers_to_continuous_file: missing fd_path");
				return false;
			}

			typedef Vfs::Directory_service::Stat_result Result;

			Vfs::Directory_service::Stat stat { };

			if (_root_fs.stat(_fd_path, stat) != Result::STAT_OK)
				return false;

			return stat.type == Vfs::Node_type::CONTINUOUS_FILE;
		};

		monitor().monitor([&]
		{
			for (;;) {

				/* number of bytes written in one iteration */
				::size_t partial_out_count = 0;

				Const_byte_range_ptr const src { (char const *)_buf + _offset,
				                                  _count };

				_out_result = _handle->fs().write(_handle, src, partial_out_count);

				if (_out_result == Result::WRITE_ERR_WOULD_BLOCK)
					return Fn::INCOMPLETE;

				if (_out_result != Result::WRITE_OK)
					return Fn::COMPLETE;

				/* increment byte count reported to caller */
				_out_count += partial_out_count;

				bool const write_complete = (partial_out_count == _count);
				if (write_complete)
					return Fn::COMPLETE;

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
					return Fn::COMPLETE;
				}

				_iteration++;

				bool const stalled = (partial_out_count == 0);
				if (stalled) {
					return Fn::INCOMPLETE;
				}

				/* issue new write operation for remaining bytes */
				_count  -= partial_out_count;
				_offset += partial_out_count;
				_handle->advance_seek(partial_out_count);
			}
		});

		/* XXX reset seek pointer after loop (will be advanced below by out_count) */
		handle->seek(initial_seek);
	}

	Plugin::resume_all();

	switch (out_result) {
	case Result::WRITE_ERR_WOULD_BLOCK: return Errno(EWOULDBLOCK);
	case Result::WRITE_ERR_INVALID:     return Errno(EINVAL);
	case Result::WRITE_ERR_IO:          return Errno(EIO);
	case Result::WRITE_OK:              break;
	}

	handle->advance_seek(out_count);
	fd->modified = true;

	return out_count;
}


ssize_t Libc::Vfs_plugin::read(File_descriptor *fd, void *buf,
                               ::size_t count)
{
	if ((fd->flags & O_ACCMODE) == O_WRONLY) {
		return Errno(EBADF);
	}

	typedef Vfs::File_io_service::Read_result Result;

	Vfs::Vfs_handle *handle = vfs_handle(fd);

	if (fd->flags & O_DIRECTORY)
		return Errno(EISDIR);

	/* TODO refactor multiple monitor() calls to state machine in one call */
	bool succeeded = false;
	int result_errno = 0;
	monitor().monitor([&] {
		if (fd->flags & O_NONBLOCK && !read_ready_from_kernel(fd)) {
			result_errno = EAGAIN;
			return Fn::COMPLETE;
		}
		succeeded = true;
		return handle->fs().queue_read(handle, count) ? Fn::COMPLETE : Fn::INCOMPLETE;
	});

	if (!succeeded)
		return Errno(result_errno);

	::size_t out_count = 0;
	Result   out_result;

	monitor().monitor([&] {
		Byte_range_ptr const dst { (char *)buf, count };
		out_result = handle->fs().complete_read(handle, dst, out_count);
		return out_result != Result::READ_QUEUED ? Fn::COMPLETE : Fn::INCOMPLETE;
	});

	Plugin::resume_all();

	switch (out_result) {
	case Result::READ_ERR_WOULD_BLOCK: return Errno(EWOULDBLOCK);
	case Result::READ_ERR_INVALID:     return Errno(EINVAL);
	case Result::READ_ERR_IO:          return Errno(EIO);
	case Result::READ_OK:              break;

	case Result::READ_QUEUED: /* handled above, so never reached */ break;
	}

	handle->advance_seek(out_count);

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

	/* TODO refactor multiple monitor() calls to state machine in one call */

	monitor().monitor([&] {
		return handle->fs().queue_read(handle, sizeof(Dirent)) ? Fn::COMPLETE : Fn::INCOMPLETE;
	});

	Result   out_result;
	::size_t out_count;

	monitor().monitor([&] {
		Byte_range_ptr const dst { (char *)&dirent_out, sizeof(Dirent) };
		out_result = handle->fs().complete_read(handle, dst, out_count);
		return out_result != Result::READ_QUEUED ? Fn::COMPLETE : Fn::INCOMPLETE;
	});

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
		case Dirent_type::TRANSACTIONAL_FILE: return DT_CHR;
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

	Genode::copy_cstring(dirent.d_name, dirent_out.name.buf, sizeof(dirent.d_name));

	dirent.d_namlen = Genode::strlen(dirent.d_name);

	/*
	 * Keep track of VFS seek pointer and user-supplied basep.
	 */
	handle->advance_seek(sizeof(Vfs::Directory_service::Dirent));

	*basep += sizeof(struct dirent);

	return sizeof(struct dirent);
}


Libc::Vfs_plugin::Ioctl_result
Libc::Vfs_plugin::_ioctl_tio(File_descriptor *fd, unsigned long request, char *argp)
{
	if (!argp)
		return { true, EINVAL };

	bool handled = false;

	if (request == TIOCGWINSZ) {

		monitor().monitor([&] {
			_with_info(*fd, [&] (Xml_node info) {
				if (info.type() == "terminal") {
					::winsize *winsize = (::winsize *)argp;
					winsize->ws_row = info.attribute_value("rows",    25U);
					winsize->ws_col = info.attribute_value("columns", 80U);
					handled = true;
				}
			});

			return Fn::COMPLETE;
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

	return { handled, 0 };
}


Libc::Vfs_plugin::Ioctl_result
Libc::Vfs_plugin::_ioctl_dio(File_descriptor *fd, unsigned long request, char *argp)
{
	if (!argp)
		return { true, EINVAL };

	bool handled = false;

	using ::off_t;

	if (request == DIOCGMEDIASIZE) {

		monitor().monitor([&] {
			_with_info(*fd, [&] (Xml_node info) {
				if (info.type() == "block") {

					size_t const size =
						info.attribute_value("size", 0UL);
					if (!size) {
						warning("block size is 0");
					}

					uint64_t const count =
						info.attribute_value("count", 0ULL);
					if (!count) {
						warning("block count is 0");
					}

					off_t disk_size = (off_t) count * size;
					if (disk_size < 0) {
						warning("disk size overflow - returning 0");
						disk_size = 0;
					}

					*(off_t*)argp = disk_size;
					handled = true;
				}
			});

			return Fn::COMPLETE;
		});

	}

	return { handled, 0 };
}


Libc::Vfs_plugin::Ioctl_result
Libc::Vfs_plugin::_ioctl_sndctl(File_descriptor *fd, unsigned long request, char *argp)
{
	bool handled = false;
	/*
	 * Initialize to "success" and any ioctl is required to set
	 * in case of error.
	 *
	 * This method will either return handled equals true if the I/O control
	 * was handled successfully or failed and the result is not successful
	 * (see the end of this method).
	 * 
	 */
	int result = 0;

	if (request == OSS_GETVERSION) {

		if (!argp) return { true, EINVAL };

		*(int *)argp = SOUND_VERSION;

		handled = true;

	} else if (request == SNDCTL_DSP_CHANNELS) {

		if (!argp) return { true, EINVAL };

		monitor().monitor([&] {
			_with_info(*fd, [&] (Xml_node info) {
				if (info.type() != "oss") {
					return;
				}

				unsigned int const avail_chans =
					info.attribute_value("channels", 0U);
				if (avail_chans == 0U) {
					result = EINVAL;
					return;
				}

				int const num_chans = *(int const *)argp;
				if (num_chans < 0) {
					result = EINVAL;
					return;
				}

				if ((unsigned)num_chans != avail_chans) {
					result = ENOTSUP;
					return;
				}

				*(int *)argp = avail_chans;

				handled = true;
			});

			return Fn::COMPLETE;
		});

	} else if (request == SNDCTL_DSP_CURRENT_OPTR) {

		if (!argp) return { true, EINVAL };

		monitor().monitor([&] {
			_with_info(*fd, [&] (Xml_node info) {

				if (info.type() != "oss") {
					return;
				}

				long long const optr_samples =
					info.attribute_value("optr_samples", -1L);
				int const optr_fifo_samples =
					info.attribute_value("optr_fifo_samples", -1L);
				if ((optr_samples == -1) || (optr_fifo_samples == -1)) {
					result = ENOTSUP;
					return;
				}

				oss_count_t *optr = (oss_count_t *)argp;
				optr->samples      = optr_samples;
				optr->fifo_samples = optr_fifo_samples;

				handled = true;
			});

			return Fn::COMPLETE;
		});

	} else if (request == SNDCTL_DSP_GETERROR) {

		if (!argp) return { true, EINVAL };

		int play_underruns = 0;

		monitor().monitor([&] {
			_with_info(*fd, [&] (Xml_node info) {
				if (info.type() != "oss") {
					return;
				}

				play_underruns = info.attribute_value("play_underruns", 0U);
			});

			return Fn::COMPLETE;
		});


		if (play_underruns > 0) {

			/* reset */

			char const play_underruns_string[] = "0";
			Absolute_path play_underruns_path = ioctl_dir(*fd);
			play_underruns_path.append_element("play_underruns");
			File_descriptor *play_underruns_fd = open(play_underruns_path.base(), O_RDWR);
			if (!play_underruns_fd)
				return { true, ENOTSUP };
			write(play_underruns_fd, play_underruns_string, sizeof(play_underruns_string));
			close(play_underruns_fd);
		}

		struct audio_errinfo *err_info =
			(struct audio_errinfo *)argp;

		err_info->play_underruns  = play_underruns;
		err_info->rec_overruns    = 0;
		err_info->play_ptradjust  = 0;
		err_info->rec_ptradjust   = 0;
		err_info->play_errorcount = 0;
		err_info->rec_errorcount  = 0;
		err_info->play_lasterror  = 0;
		err_info->rec_lasterror   = 0;
		err_info->play_errorparm  = 0;
		err_info->rec_errorparm   = 0;

		handled = true;

	} else if (request == SNDCTL_DSP_GETFMTS) {

		if (!argp) return { true, EINVAL };

		monitor().monitor([&] {
			_with_info(*fd, [&] (Xml_node info) {
				if (info.type() != "oss") {
					return;
				}

				unsigned int const format =
					info.attribute_value("format", 0U);
				if (format == 0U) {
					result = EINVAL;
					return;
				}

				*(int *)argp = format;

				handled = true;
			});

			return Fn::COMPLETE;
		});

	} else if (request == SNDCTL_DSP_GETISPACE) {

		if (!argp) return { true, EINVAL };

		monitor().monitor([&] {
			_with_info(*fd, [&] (Xml_node info) {
				if (info.type() != "oss") {
					return;
				}

				unsigned int const ifrag_size =
					info.attribute_value("ifrag_size", 0U);
				unsigned int const ifrag_avail =
					info.attribute_value("ifrag_avail", 0U);
				unsigned int const ifrag_total =
					info.attribute_value("ifrag_total", 0U);
				unsigned int const ifrag_bytes =
					info.attribute_value("ifrag_bytes", 0U);
				if (!ifrag_size || !ifrag_total) {
					result = ENOTSUP;
					return;
				}

				int const fragments  = (int)ifrag_avail;
				int const fragstotal = (int)ifrag_total;
				int const fragsize   = (int)ifrag_size;
				int const bytes      = (int)ifrag_bytes;
				if (fragments < 0 || fragstotal < 0 ||
				    fragsize < 0 || bytes < 0) {
					result = EINVAL;
					return;
				}

				struct audio_buf_info *buf_info =
					(struct audio_buf_info *)argp;

				buf_info->fragments  = fragments;
				buf_info->fragstotal = fragstotal;
				buf_info->fragsize   = fragsize;
				buf_info->bytes      = bytes;

				handled = true;
			});

			return Fn::COMPLETE;
		});

	} else if (request == SNDCTL_DSP_GETOPTR) {

		if (!argp) return { true, EINVAL };

		/* dummy implementation */

		count_info &ci = *(count_info *)argp;
		ci = {
			.bytes  = 0, /* Total # of bytes processed */
			.blocks = 0, /* # of fragment transitions since last time */
			.ptr    = 0, /* Current DMA pointer value */
		};

		handled = true;

	} else if (request == SNDCTL_DSP_GETOSPACE) {

		if (!argp) return { true, EINVAL };

		monitor().monitor([&] {
			_with_info(*fd, [&] (Xml_node info) {
				if (info.type() != "oss") {
					return;
				}

				unsigned int const ofrag_size =
					info.attribute_value("ofrag_size", 0U);
				unsigned int const ofrag_avail =
					info.attribute_value("ofrag_avail", 0U);
				unsigned int const ofrag_total =
					info.attribute_value("ofrag_total", 0U);
				unsigned int const ofrag_bytes =
					info.attribute_value("ofrag_bytes", 0U);
				if (!ofrag_size || !ofrag_total) {
					result = ENOTSUP;
					return;
				}

				int const fragments  = (int)ofrag_avail;
				int const fragstotal = (int)ofrag_total;
				int const fragsize   = (int)ofrag_size;
				int const bytes      = (int)ofrag_bytes;
				if (fragments < 0 || fragstotal < 0 ||
				    fragsize < 0 || bytes < 0) {
					result = EINVAL;
					return;
				}

				struct audio_buf_info *buf_info =
					(struct audio_buf_info *)argp;

				buf_info->fragments  = fragments;
				buf_info->fragstotal = fragstotal;
				buf_info->fragsize   = fragsize;
				buf_info->bytes      = bytes;

				handled = true;
			});

			return Fn::COMPLETE;
		});

	} else if (request == SNDCTL_DSP_GETPLAYVOL) {

		if (!argp) return { true, EINVAL };

		/* dummy implementation */

		int *vol = (int *)argp;

		*vol = 100;

		handled = true;

	} else if (request == SNDCTL_DSP_LOW_WATER) {

		if (!argp) return { true, EINVAL };

		/* dummy implementation */

		int *val = (int *)argp;

		*val = 0;

		handled = true;

	} else if (request == SNDCTL_DSP_NONBLOCK) {

		/* dummy implementation */

		handled = true;

	} else if (request == SNDCTL_DSP_POST) {

		handled = true;

	} else if (request == SNDCTL_DSP_HALT) {

		if (((fd->flags & O_ACCMODE) == O_RDONLY) ||
		    ((fd->flags & O_ACCMODE) == O_RDWR)) {

			char const halt_input_string[] = "1";

			Absolute_path halt_input_path = ioctl_dir(*fd);
			halt_input_path.append_element("halt_input");
			File_descriptor *halt_input_fd = open(halt_input_path.base(), O_WRONLY);
			if (!halt_input_fd)
				return { true, ENOTSUP };
			write(halt_input_fd, halt_input_string, sizeof(halt_input_string));
			close(halt_input_fd);
		}

		if (((fd->flags & O_ACCMODE) == O_WRONLY) ||
		    ((fd->flags & O_ACCMODE) == O_RDWR)) {

			char const halt_output_string[] = "1";

			Absolute_path halt_output_path = ioctl_dir(*fd);
			halt_output_path.append_element("halt_output");
			File_descriptor *halt_output_fd = open(halt_output_path.base(), O_WRONLY);
			if (!halt_output_fd)
				return { true, ENOTSUP };
			write(halt_output_fd, halt_output_string, sizeof(halt_output_string));
			close(halt_output_fd);
		}

		handled = true;

	} else if (request == SNDCTL_DSP_SAMPLESIZE) {

		if (!argp) return { true, EINVAL };

		monitor().monitor([&] {
			_with_info(*fd, [&] (Xml_node info) {
				if (info.type() != "oss") {
					return;
				}

				unsigned int const format =
					info.attribute_value("format", ~0U);
				if (format == ~0U) {
					result = EINVAL;
					return;
				}

				int const requested_fmt = *(int const *)argp;

				if (requested_fmt != (int)format) {
					result = ENOTSUP;
					return;
				}

				handled = true;
			});

			return Fn::COMPLETE;
		});

	} else if (request == SNDCTL_DSP_SETFRAGMENT) {

		if (!argp) return { true, EINVAL };

		int *frag = (int *)argp;
		int max_fragments = *frag >> 16;
		int size_selector = *frag & ((1<<16) - 1);

		if (((fd->flags & O_ACCMODE) == O_RDONLY) ||
		    ((fd->flags & O_ACCMODE) == O_RDWR)) {

			char ifrag_total_string[16];
			char ifrag_size_string[16];

			::snprintf(ifrag_total_string, sizeof(ifrag_total_string),
		           	   "%u", max_fragments);

			::snprintf(ifrag_size_string, sizeof(ifrag_size_string),
		           	   "%u", 1 << size_selector);

			Absolute_path ifrag_total_path = ioctl_dir(*fd);
			ifrag_total_path.append_element("ifrag_total");
			File_descriptor *ifrag_total_fd = open(ifrag_total_path.base(), O_RDWR);
			if (!ifrag_total_fd)
				return { true, ENOTSUP };
			write(ifrag_total_fd, ifrag_total_string, sizeof(ifrag_total_string));
			close(ifrag_total_fd);

			Absolute_path ifrag_size_path = ioctl_dir(*fd);
			ifrag_size_path.append_element("ifrag_size");
			File_descriptor *ifrag_size_fd = open(ifrag_size_path.base(), O_RDWR);
			if (!ifrag_size_fd)
				return { true, ENOTSUP };
			write(ifrag_size_fd, ifrag_size_string, sizeof(ifrag_size_string));
			close(ifrag_size_fd);

			monitor().monitor([&] {

				_with_info(*fd, [&] (Xml_node info) {
					if (info.type() != "oss") {
						return;
					}

					unsigned int const ifrag_size =
						info.attribute_value("ifrag_size", 0U);
					unsigned int const ifrag_size_log2 =
						ifrag_size ? Genode::log2(ifrag_size) : 0;

					unsigned int const ifrag_total =
						info.attribute_value("ifrag_total", 0U);

					if (!ifrag_total || !ifrag_size_log2) {
						result = ENOTSUP;
						return;
					}
				});
				
				return Fn::COMPLETE;
			});
		}

		if (((fd->flags & O_ACCMODE) == O_WRONLY) ||
		    ((fd->flags & O_ACCMODE) == O_RDWR)) {

			char ofrag_total_string[16];
			char ofrag_size_string[16];

			::snprintf(ofrag_total_string, sizeof(ofrag_total_string),
		           	   "%u", max_fragments);

			::snprintf(ofrag_size_string, sizeof(ofrag_size_string),
		           	   "%u", 1 << size_selector);

			Absolute_path ofrag_total_path = ioctl_dir(*fd);
			ofrag_total_path.append_element("ofrag_total");
			File_descriptor *ofrag_total_fd = open(ofrag_total_path.base(), O_RDWR);
			if (!ofrag_total_fd)
				return { true, ENOTSUP };
			write(ofrag_total_fd, ofrag_total_string, sizeof(ofrag_total_string));
			close(ofrag_total_fd);

			Absolute_path ofrag_size_path = ioctl_dir(*fd);
			ofrag_size_path.append_element("ofrag_size");
			File_descriptor *ofrag_size_fd = open(ofrag_size_path.base(), O_RDWR);
			if (!ofrag_size_fd)
				return { true, ENOTSUP };
			write(ofrag_size_fd, ofrag_size_string, sizeof(ofrag_size_string));
			close(ofrag_size_fd);

			monitor().monitor([&] {

				_with_info(*fd, [&] (Xml_node info) {
					if (info.type() != "oss") {
						return;
					}

					unsigned int const ofrag_size =
						info.attribute_value("ofrag_size", 0U);
					unsigned int const ofrag_size_log2 =
						ofrag_size ? Genode::log2(ofrag_size) : 0;

					unsigned int const ofrag_total =
						info.attribute_value("ofrag_total", 0U);

					if (!ofrag_total || !ofrag_size_log2) {
						result = ENOTSUP;
						return;
					}
				});

				return Fn::COMPLETE;
			});
		}

		handled = true;

	} else if (request == SNDCTL_DSP_SETPLAYVOL) {

		if (!argp) return { true, EINVAL };

		/* dummy implementation */

		int *vol = (int *)argp;

		*vol = 100;

		handled = true;

	} else if (request == SNDCTL_DSP_SETTRIGGER) {

		if (!argp) return { true, EINVAL };

		int mask = *(int *)argp;

		if (((fd->flags & O_ACCMODE) == O_RDONLY) ||
		    ((fd->flags & O_ACCMODE) == O_RDWR)) {

			char enable_input_string[2];

			::snprintf(enable_input_string, sizeof(enable_input_string),
			           "%u", (mask & PCM_ENABLE_INPUT) ? 1 : 0);

			Absolute_path enable_input_path = ioctl_dir(*fd);
			enable_input_path.append_element("enable_input");
			File_descriptor *enable_input_fd = open(enable_input_path.base(), O_WRONLY);
			if (!enable_input_fd)
				return { true, ENOTSUP };
			write(enable_input_fd, enable_input_string, sizeof(enable_input_string));
			close(enable_input_fd);
		}

		if (((fd->flags & O_ACCMODE) == O_WRONLY) ||
		    ((fd->flags & O_ACCMODE) == O_RDWR)) {

			char enable_output_string[2];

			::snprintf(enable_output_string, sizeof(enable_output_string),
			           "%u", (mask & PCM_ENABLE_OUTPUT) ? 1 : 0);

			Absolute_path enable_output_path = ioctl_dir(*fd);
			enable_output_path.append_element("enable_output");
			File_descriptor *enable_output_fd = open(enable_output_path.base(), O_WRONLY);
			if (!enable_output_fd)
				return { true, ENOTSUP };
			write(enable_output_fd, enable_output_string, sizeof(enable_output_string));
			close(enable_output_fd);
		}

		handled = true;

	} else if (request == SNDCTL_DSP_SPEED) {

		if (!argp) return { true, EINVAL };

		monitor().monitor([&] {
			_with_info(*fd, [&] (Xml_node info) {
				if (info.type() != "oss") {
					return;
				}

				unsigned int const samplerate =
					info.attribute_value("sample_rate", 0U);
				if (samplerate == 0U) {
					result = EINVAL;
					return;
				}

				int const speed = *(int const *)argp;
				if (speed < 0) {
					result = EINVAL;
					return;
				}

				if ((unsigned)speed != samplerate) {
					result = ENOTSUP;
					return;
				}

				*(int *)argp = samplerate;

				handled = true;
			});

			return Fn::COMPLETE;
		});

	} else if (request == SNDCTL_DSP_SYNC) {

		/*
		 * SNDCTL_DSP_SYNC should be implemented like follows, but we disabled
		 * the implementation and just return for two reasons. The VirtualBox 6
		 * backend requires the sync operation to complete with very tight
		 * timing in a special thread, which our current implementation can't
		 * assure. Also, the OSS documentation (and examples) advise against the
		 * use of this feature in new programs.
		 */
		if (0) monitor().monitor([&] {

			auto result = Fn::INCOMPLETE;

			_with_info(*fd, [&] (Xml_node info) {

				if (info.type() != "oss") return;

				unsigned int const ofrag_avail =
					info.attribute_value("ofrag_avail", 0U);
				unsigned int const ofrag_total =
					info.attribute_value("ofrag_total", 0U);

				result = (ofrag_avail == ofrag_total ? Fn::COMPLETE : Fn::INCOMPLETE);
			});

			return result;
		});

		handled = true;

	} else if (request == SNDCTL_SYSINFO) {

		if (!argp) return { true, EINVAL };

		/* dummy implementation */

		oss_sysinfo *si = (oss_sysinfo *)argp;
		Genode::memset(si, 0, sizeof(*si));

		handled = true;
	}

	/*
	 * Either handled or a failed attempt will mark the I/O control
	 * as handled.
	 */
	return { handled || result != 0, result };
}


Libc::Vfs_plugin::Ioctl_result
Libc::Vfs_plugin::_ioctl_tapctl(File_descriptor *fd, unsigned long request, char *argp)
{
	bool handled = false;
	int  result  = 0;

	if (request == TAPGIFNAME) {       /* return device name */
		if (!argp)
			return { true, EINVAL };

		ifreq *ifr = reinterpret_cast<ifreq*>(argp);

		monitor().monitor([&] {
			_with_info(*fd, [&] (Xml_node info) {
				if (info.type() == "tap") {
					String<IFNAMSIZ> name = info.attribute_value("name", String<IFNAMSIZ> { });
					copy_cstring(ifr->ifr_name, name.string(), IFNAMSIZ);
					handled = true;
				}
			});

			return Fn::COMPLETE;
		});
	}
	else if (request == SIOCGIFADDR) { /* get MAC address */
		if (!argp)
			return { true, EINVAL };

		monitor().monitor([&] {
			_with_info(*fd, [&] (Xml_node info) {
				if (info.type() == "tap") {
					Net::Mac_address mac = info.attribute_value("mac_addr", Net::Mac_address { });
					mac.copy(argp);
					handled = true;
				}
			});

			return Fn::COMPLETE;
		});
	}
	else if (request == SIOCSIFADDR) { /* set MAC address */
		if (!argp)
			return { true, EINVAL };

		Net::Mac_address new_mac    { argp };
		String<18>       mac_string { new_mac };

		/* write string into file */
		Absolute_path mac_addr_path = ioctl_dir(*fd);
		mac_addr_path.append_element("mac_addr");
		File_descriptor *mac_addr_fd = open(mac_addr_path.base(), O_RDWR);
		if (!mac_addr_fd)
			return { true, ENOTSUP };
		write(mac_addr_fd, mac_string.string(), mac_string.length());
		close(mac_addr_fd);

		monitor().monitor([&] {
			/* check whether mac address changed, return ENOTSUP if not */
			_with_info(*fd, [&] (Xml_node info) {
				if (info.type() == "tap") {
					if (!info.has_attribute("mac_addr"))
						result = ENOTSUP;
					else {
						Net::Mac_address cur_mac = info.attribute_value("mac_addr", Net::Mac_address { });
						if (cur_mac != new_mac)
							result = ENOTSUP;
					}

					handled = true;
				}
			});

			return Fn::COMPLETE;
		});
	}

	return { handled, result };
}

int Libc::Vfs_plugin::ioctl(File_descriptor *fd, unsigned long request, char *argp)
{
	Ioctl_result result { false, 0 };

	/* we need libc's off_t which is int64_t */
	using ::off_t;

	switch (request) {
	case TIOCGWINSZ:
	case TIOCGETA:
		result = _ioctl_tio(fd, request, argp);
		break;
	case DIOCGMEDIASIZE:
		result = _ioctl_dio(fd, request, argp);
		break;
	case OSS_GETVERSION:
	case SNDCTL_DSP_CHANNELS:
	case SNDCTL_DSP_CURRENT_OPTR:
	case SNDCTL_DSP_GETERROR:
	case SNDCTL_DSP_GETFMTS:
	case SNDCTL_DSP_GETISPACE:
	case SNDCTL_DSP_GETOPTR:
	case SNDCTL_DSP_GETOSPACE:
	case SNDCTL_DSP_GETPLAYVOL:
	case SNDCTL_DSP_LOW_WATER:
	case SNDCTL_DSP_NONBLOCK:
	case SNDCTL_DSP_POST:
	case SNDCTL_DSP_RESET:
	case SNDCTL_DSP_SAMPLESIZE:
	case SNDCTL_DSP_SETFRAGMENT:
	case SNDCTL_DSP_SETPLAYVOL:
	case SNDCTL_DSP_SETTRIGGER:
	case SNDCTL_DSP_SPEED:
	case SNDCTL_DSP_SYNC:
	case SNDCTL_SYSINFO:
		result = _ioctl_sndctl(fd, request, argp);
		break;
	case TAPSIFINFO:
	case TAPGIFINFO:
	case TAPSDEBUG:
	case TAPGDEBUG:
	case TAPGIFNAME:
	case SIOCGIFADDR:
	case SIOCSIFADDR:
		result = _ioctl_tapctl(fd, request, argp);
		break;
	default:
		break;
	}

	if (!result.handled)
		return Errno(EINVAL);

	return result.error ? Errno(result.error) : 0;
}


/* it's always SEEK_SET */
::off_t Libc::Vfs_plugin::lseek_from_kernel(File_descriptor *fd, ::off_t offset)
{
	Vfs::Vfs_handle *handle = vfs_handle(fd);

	handle->seek(offset);
	return handle->seek();
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
	Sync sync { *handle, _update_mtime, _current_real_time };

	bool succeeded = false;
	int result_errno = 0;
	monitor().monitor([&] {
		if (fd->modified) {
			if (!sync.complete()) {
				return Fn::INCOMPLETE;
			}
			fd->modified = false;
		}

		typedef Vfs::File_io_service::Ftruncate_result Result;

		switch (handle->fs().ftruncate(handle, length)) {
		case Result::FTRUNCATE_ERR_NO_PERM:   result_errno = EPERM;  break;
		case Result::FTRUNCATE_ERR_INTERRUPT: result_errno = EINTR;  break;
		case Result::FTRUNCATE_ERR_NO_SPACE:  result_errno = ENOSPC; break;
		case Result::FTRUNCATE_OK:        succeeded = true;   break;
		}
		return Fn::COMPLETE;
	});
	return succeeded ? 0 : Errno(result_errno);
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

	if (!fd->modified)
		return 0;

	Sync sync { *handle, _update_mtime, _current_real_time };

	monitor().monitor([&] {
		if (!sync.complete()) {
			return Fn::INCOMPLETE;
		}
		return Fn::COMPLETE;
	});

	return 0;
}


int Libc::Vfs_plugin::symlink(const char *target_path, const char *link_path)
{
	Vfs::Vfs_handle *handle    { nullptr };
	Constructible<Sync> sync;

	size_t const count = ::strlen(target_path) + 1;

	size_t out_count = 0;

	{
		bool succeeded { false };
		int result_errno { 0 };
		monitor().monitor([&] {

			typedef Vfs::Directory_service::Openlink_result Openlink_result;

			Openlink_result openlink_result =
				_root_fs.openlink(link_path, true, &handle, _alloc);

			switch (openlink_result) {
			case Openlink_result::OPENLINK_ERR_LOOKUP_FAILED:
				result_errno = ENOENT; return Fn::COMPLETE;
			case Openlink_result::OPENLINK_ERR_NAME_TOO_LONG:
				result_errno = ENAMETOOLONG; return Fn::COMPLETE;
			case Openlink_result::OPENLINK_ERR_NODE_ALREADY_EXISTS:
				result_errno = EEXIST; return Fn::COMPLETE;
			case Openlink_result::OPENLINK_ERR_NO_SPACE:
				result_errno = ENOSPC; return Fn::COMPLETE;
			case Openlink_result::OPENLINK_ERR_OUT_OF_RAM:
				result_errno = ENOSPC; return Fn::COMPLETE;
			case Openlink_result::OPENLINK_ERR_OUT_OF_CAPS:
				result_errno = ENOSPC; return Fn::COMPLETE;
			case Vfs::Directory_service::OPENLINK_ERR_PERMISSION_DENIED:
				result_errno = EPERM; return Fn::COMPLETE;
			case Openlink_result::OPENLINK_OK:
				break;
			}

			handle->handler(&_response_handler);
			succeeded = true;
			return Fn::COMPLETE;
		});

		if (!succeeded)
			return Errno(result_errno);
	}

	/* must be done outside the monitor because constructor needs libc I/O */
	sync.construct(*handle, _update_mtime, _current_real_time);

	{
		bool succeeded { false };
		int result_errno { 0 };
		enum class Stage { WRITE, SYNC } stage = Stage::WRITE;

		monitor().monitor([&] {

			switch (stage) {

			case Stage::WRITE:
				{
					using Result = Vfs::File_io_service::Write_result;

					Const_byte_range_ptr const src { target_path, count };

					Result result = handle->fs().write(handle, src, out_count);

					if (result == Result::WRITE_ERR_WOULD_BLOCK)
						return Fn::INCOMPLETE;
				}
				stage = Stage::SYNC;
				[[fallthrough]];

			case Stage::SYNC:
				{
					if (!sync->complete())
						return Fn::INCOMPLETE;
					handle->close();
				} break;
			}

			if (out_count != count)
				result_errno = ENAMETOOLONG;
			else
				succeeded = true;
			return Fn::COMPLETE;
		});

		if (!succeeded)
		return Errno(result_errno);
	}

	return 0;
}


ssize_t Libc::Vfs_plugin::readlink(const char *link_path, char *buf, ::size_t buf_size)
{
	enum class Stage { OPEN, QUEUE_READ, COMPLETE_READ };

	Stage stage { Stage::OPEN };

	Vfs::Vfs_handle *handle = nullptr;

	::size_t out_count    = 0;
	bool     succeeded    = false;
	int      result_errno = 0;

	monitor().monitor([&] {

		switch (stage) {
		case Stage::OPEN:
			{
				typedef Vfs::Directory_service::Openlink_result Openlink_result;

				Openlink_result openlink_result =
					_root_fs.openlink(link_path, false, &handle, _alloc);

				switch (openlink_result) {
				case Openlink_result::OPENLINK_ERR_LOOKUP_FAILED:
					result_errno = ENOENT; return Fn::COMPLETE;
				case Openlink_result::OPENLINK_ERR_NAME_TOO_LONG:
					/* should not happen */
					result_errno = ENAMETOOLONG; return Fn::COMPLETE;
				case Openlink_result::OPENLINK_ERR_NODE_ALREADY_EXISTS:
				case Openlink_result::OPENLINK_ERR_NO_SPACE:
				case Openlink_result::OPENLINK_ERR_OUT_OF_RAM:
				case Openlink_result::OPENLINK_ERR_OUT_OF_CAPS:
				case Openlink_result::OPENLINK_ERR_PERMISSION_DENIED:
					result_errno = EACCES; return Fn::COMPLETE;
				case Openlink_result::OPENLINK_OK:
					break;
				}

				handle->handler(&_response_handler);
			} stage = Stage::QUEUE_READ; [[ fallthrough ]];

		case Stage::QUEUE_READ:
			{
				if (!handle->fs().queue_read(handle, buf_size))
					return Fn::INCOMPLETE;
			} stage = Stage::COMPLETE_READ; [[ fallthrough ]];

		case Stage::COMPLETE_READ:
			{
				using Result = Vfs::File_io_service::Read_result;

				Byte_range_ptr const dst { buf, buf_size };

				Result out_result =
					handle->fs().complete_read(handle, dst, out_count);

				switch (out_result) {
				case Result::READ_QUEUED: return Fn::INCOMPLETE;;

				case Result::READ_ERR_WOULD_BLOCK: result_errno = EWOULDBLOCK; break;
				case Result::READ_ERR_INVALID:     result_errno = EINVAL;      break;
				case Result::READ_ERR_IO:          result_errno = EIO;         break;
				case Result::READ_OK:              succeeded = true;           break;
				};
				handle->close();
			} break;
		}

		return Fn::COMPLETE;
	});

	if (!succeeded)
		return Errno(result_errno);

	return out_count;
}


int Libc::Vfs_plugin::rmdir(char const *path)
{
	return unlink(path);
}


int Libc::Vfs_plugin::unlink(char const *path)
{
	typedef Vfs::Directory_service::Unlink_result Result;

	bool succeeded = false;
	int result_errno = 0;
	monitor().monitor([&] {
		switch (_root_fs.unlink(path)) {
		case Result::UNLINK_ERR_NO_ENTRY:  result_errno = ENOENT;    break;
		case Result::UNLINK_ERR_NO_PERM:   result_errno = EPERM;     break;
		case Result::UNLINK_ERR_NOT_EMPTY: result_errno = ENOTEMPTY; break;
		case Result::UNLINK_OK:        succeeded = true;      break;
		}
		return Fn::COMPLETE;
	});
	if (!succeeded)
		return Errno(result_errno);

	return 0;
}


int Libc::Vfs_plugin::rename(char const *from_path, char const *to_path)
{
	typedef Vfs::Directory_service::Rename_result Result;

	bool succeeded = false;
	int result_errno = false;
	monitor().monitor([&] {
		if (_root_fs.leaf_path(to_path)) {
			if (_root_fs.directory(to_path)) {
				if (!_root_fs.directory(from_path)) {
					result_errno = EISDIR; return Fn::COMPLETE;
				}

				if (_root_fs.num_dirent(to_path)) {
					result_errno = ENOTEMPTY; return Fn::COMPLETE;
				}

			} else {
				if (_root_fs.directory(from_path)) {
					result_errno = ENOTDIR; return Fn::COMPLETE;
				}
			}
		}

		switch (_root_fs.rename(from_path, to_path)) {
		case Result::RENAME_ERR_NO_ENTRY: result_errno = ENOENT; break;
		case Result::RENAME_ERR_CROSS_FS: result_errno = EXDEV;  break;
		case Result::RENAME_ERR_NO_PERM:  result_errno = EPERM;  break;
		case Result::RENAME_OK:       succeeded = true;   break;
		}
		return Fn::COMPLETE;
	});

	if (!succeeded)
		return Errno(result_errno);

	return 0;
}


void *Libc::Vfs_plugin::mmap(void *addr_in, ::size_t length, int prot, int flags,
                             File_descriptor *fd, ::off_t offset)
{
	if ((prot != PROT_READ) && (prot != (PROT_READ | PROT_WRITE))) {
		error("mmap for prot=", Hex(prot), " not supported");
		errno = EACCES;
		return MAP_FAILED;
	}

	if (flags & MAP_FIXED) {
		error("mmap for fixed predefined address not supported yet");
		errno = EINVAL;
		return MAP_FAILED;
	}

	void *addr = nullptr;

	if (flags & MAP_PRIVATE) {

		/*
		 * XXX attempt to obtain memory mapping via
		 *     'Vfs::Directory_service::dataspace'.
		 */

		addr = mem_alloc()->alloc(length, PAGE_SHIFT);
		if (addr == (void *)-1) {
			error("mmap out of memory");
			errno = ENOMEM;
			return MAP_FAILED;
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
				return MAP_FAILED;
			} else if (length_read == 0) /* EOF */
				break; /* done (length can legally be greater than the file length) */
			read_remain -= length_read;
			read_offset += length_read;
			read_addr += length_read;
		}

	} else if (flags & MAP_SHARED) {

		/* create another VFS handle to keep the file open as long as the mapping exists */

		Vfs::Vfs_handle *reference_handle = nullptr;
		typedef Vfs::Directory_service::Open_result Result;
		Result vfs_open_result;
		monitor().monitor([&] {
			vfs_open_result = _root_fs.open(fd->fd_path, fd->flags,
			                                &reference_handle, _alloc);
			return Fn::COMPLETE;
		});

		if (vfs_open_result != Result::OPEN_OK) {
			error("mmap could not create reference VFS handle");
			errno = ENFILE;
			return MAP_FAILED;
		}

		Genode::Dataspace_capability ds_cap;

		monitor().monitor([&] {
			ds_cap = _root_fs.dataspace(fd->fd_path);
			return Fn::COMPLETE;
		});

		if (!ds_cap.valid()) {
			Genode::error("mmap got invalid dataspace capability");
			monitor().monitor([&] {
				reference_handle->close();
				return Fn::COMPLETE;
			});
			errno = ENODEV;
			return MAP_FAILED;
		}

		try {
			addr = region_map().attach(ds_cap, length, offset);
		} catch (...) {
			monitor().monitor([&] {
				reference_handle->close();
				return Fn::COMPLETE;
			});
			errno = ENOMEM;
			return MAP_FAILED;
		}

		new (_alloc) Mmap_entry(_mmap_registry, addr, reference_handle);
	}

	return addr;
}


int Libc::Vfs_plugin::munmap(void *addr, ::size_t)
{
	using Size_at_error = Mem_alloc::Size_at_error;

	Mem_alloc::Size_at_result const size_at_result = mem_alloc()->size_at(addr);

	if (size_at_result.ok()) {
		/* private mapping */
		size_at_result.with_result(
			[&] (size_t)        { mem_alloc()->free(addr); },
			[&] (Size_at_error) {                          });

		return 0;
	}

	/* return error if addr is not a block start address */
	if (size_at_result == Size_at_error::MISMATCHING_ADDR)
		return Errno(EINVAL);

	/* shared mapping */

	Vfs::Vfs_handle *reference_handle = nullptr;

	_mmap_registry.for_each([&] (Mmap_entry &entry) {
		if (entry.start == addr) {
			reference_handle = entry.reference_handle;
			destroy(_alloc, &entry);
			region_map().detach(addr);
		}
	});

	if (!reference_handle)
		return Errno(EINVAL);

	monitor().monitor([&] {
		reference_handle->close();
		return Fn::COMPLETE;
	});

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

		meta_fd = open(new_path.base(), O_RDONLY);
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

		File_descriptor *fd = open(path.base(), flags);
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


int Libc::Vfs_plugin::poll(Pollfd fds[], int nfds)
{
	int nready = 0;

	auto fn = [&] {

		for (int pollfd_index = 0; pollfd_index < nfds; pollfd_index++) {

			Vfs::Vfs_handle *handle = vfs_handle(fds[pollfd_index].fdo);
			if (!handle) continue;

			bool fd_ready = false;

			if (fds[pollfd_index].events & (POLLIN | POLLPRI | POLLRDNORM | POLLRDBAND)) {
				if (handle->fs().read_ready(*handle)) {
					*fds[pollfd_index].revents |= POLLIN;
					fd_ready = true;
				} else {
					handle->fs().notify_read_ready(handle);
				}
			}

			if (fds[pollfd_index].events & (POLLOUT | POLLWRNORM | POLLWRBAND)) {
				if (handle->fs().write_ready(*handle)) {
					*fds[pollfd_index].revents |= POLLOUT;
					fd_ready = true;
				}
			}

			/* XXX POLLERR not supported */

			if (fd_ready)
				nready++;
		}
		return Fn::COMPLETE;
	};

	if (Libc::Kernel::kernel().main_context() && Libc::Kernel::kernel().main_suspended()) {
		fn();
	} else {
		monitor().monitor(fn);
	}

	return nready;
}
