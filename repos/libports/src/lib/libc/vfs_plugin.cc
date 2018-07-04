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
#include <vfs_plugin.h>

/* libc-internal includes */
#include "libc_mem_alloc.h"
#include "libc_errno.h"
#include "task.h"

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

	Genode::memset(dst, 0, sizeof(*dst));

	dst->st_uid     = src.uid;
	dst->st_gid     = src.gid;
	dst->st_mode    = src.mode;
	dst->st_size    = src.size;
	dst->st_blksize = FS_BLOCK_SIZE;
	dst->st_blocks  = (dst->st_size + FS_BLOCK_SIZE - 1) / FS_BLOCK_SIZE;
	dst->st_ino     = src.inode;
	dst->st_dev     = src.device;
}


static Genode::Xml_node *_config_node;

char const *libc_resolv_path;


namespace Libc {

	Genode::Xml_node config() __attribute__((weak));
	Genode::Xml_node config()
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

			char _buf[Vfs::MAX_PATH_LEN];

		public:

			Config_attr(char const *attr_name, char const *default_value)
			{
				Genode::strncpy(_buf, default_value, sizeof(_buf));
				try {
					Libc::config().attribute(attr_name).value(_buf, sizeof(_buf));
				} catch (...) { }
			}

			char const *string() const { return _buf; }
	};

	char const *config_rtc() __attribute__((weak));
	char const *config_rtc()
	{
		static Config_attr rtc("rtc", "");
		return rtc.string();
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

	void libc_config_init(Genode::Xml_node node)
	{
		static Genode::Xml_node config = node;
		_config_node = &config;

		libc_resolv_path = config_nameserver_file();
	}

	void notify_read_ready(Vfs::Vfs_handle *handle)
	{
		struct Check : Libc::Suspend_functor
		{
			Vfs::Vfs_handle *handle;
			Check(Vfs::Vfs_handle *handle) : handle(handle) { }
			bool suspend() override {
				return !VFS_THREAD_SAFE(handle->fs().notify_read_ready(handle)); }
		} check(handle);

		while (!VFS_THREAD_SAFE(handle->fs().notify_read_ready(handle)))
			Libc::suspend(check);
	}

	bool read_ready(Libc::File_descriptor *fd)
	{
		Vfs::Vfs_handle *handle = vfs_handle(fd);
		if (!handle) return false;

		notify_read_ready(handle);

		return VFS_THREAD_SAFE(handle->fs().read_ready(handle));
	}

}

int Libc::Vfs_plugin::access(const char *path, int amode)
{
	if (VFS_THREAD_SAFE(_root_dir.leaf_path(path)))
		return 0;

	errno = ENOENT;
	return -1;
}


Libc::File_descriptor *Libc::Vfs_plugin::open(char const *path, int flags,
                                              int libc_fd)
{
	if (VFS_THREAD_SAFE(_root_dir.directory(path))) {

		if (((flags & O_ACCMODE) != O_RDONLY)) {
			errno = EISDIR;
			return nullptr;
		}

		flags |= O_DIRECTORY;

		Vfs::Vfs_handle *handle = 0;

		typedef Vfs::Directory_service::Opendir_result Opendir_result;

		switch (VFS_THREAD_SAFE(_root_dir.opendir(path, false, &handle, _alloc))) {
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

		Libc::File_descriptor *fd =
			Libc::file_descriptor_allocator()->alloc(this, vfs_context(handle), libc_fd);

		/* FIXME error cleanup code leaks resources! */

		if (!fd) {
			errno = EMFILE;
			return nullptr;
		}

		fd->flags = flags & O_ACCMODE;

		return fd;
	}

	if (flags & O_DIRECTORY) {
		errno = ENOTDIR;
		return nullptr;
	}

	typedef Vfs::Directory_service::Open_result Result;

	Vfs::Vfs_handle *handle = 0;

	while (handle == 0) {

		switch (VFS_THREAD_SAFE(_root_dir.open(path, flags, &handle, _alloc))) {

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
				switch (VFS_THREAD_SAFE(_root_dir.open(path, flags | O_EXCL, &handle, _alloc))) {

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

	Libc::File_descriptor *fd =
		Libc::file_descriptor_allocator()->alloc(this, vfs_context(handle), libc_fd);

	/* FIXME error cleanup code leaks resources! */

	if (!fd) {
		errno = EMFILE;
		return nullptr;
	}

	fd->flags = flags & (O_ACCMODE|O_NONBLOCK|O_APPEND);

	if ((flags & O_TRUNC) && (ftruncate(fd, 0) == -1)) {
		errno = EINVAL; /* XXX which error code fits best ? */
		return nullptr;
	}

	return fd;
}


int Libc::Vfs_plugin::close(Libc::File_descriptor *fd)
{
	Vfs::Vfs_handle *handle = vfs_handle(fd);
	_vfs_sync(handle);
	VFS_THREAD_SAFE(handle->close());
	Libc::file_descriptor_allocator()->free(fd);
	return 0;
}


int Libc::Vfs_plugin::dup2(Libc::File_descriptor *fd,
                           Libc::File_descriptor *new_fd)
{
	new_fd->context = fd->context;
	return new_fd->libc_fd;
}


int Libc::Vfs_plugin::fstat(Libc::File_descriptor *fd, struct stat *buf)
{
	Vfs::Vfs_handle *handle = vfs_handle(fd);
	_vfs_sync(handle);
	return stat(fd->fd_path, buf);
}


int Libc::Vfs_plugin::fstatfs(Libc::File_descriptor *fd, struct statfs *buf)
{
	if (!fd || !buf)
		return Libc::Errno(EFAULT);

	Genode::memset(buf, 0, sizeof(*buf));

	buf->f_flags = MNT_UNION;
	return 0;
}


int Libc::Vfs_plugin::mkdir(const char *path, mode_t mode)
{
	Vfs::Vfs_handle *dir_handle { 0 };

	typedef Vfs::Directory_service::Opendir_result Opendir_result;

	switch (VFS_THREAD_SAFE(_root_dir.opendir(path, true, &dir_handle, _alloc))) {
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

	switch (VFS_THREAD_SAFE(_root_dir.stat(path, stat))) {
	case Result::STAT_ERR_NO_ENTRY: errno = ENOENT; return -1;
	case Result::STAT_ERR_NO_PERM:  errno = EACCES; return -1;
	case Result::STAT_OK:                           break;
	}

	vfs_stat_to_libc_stat_struct(stat, buf);
	return 0;
}


ssize_t Libc::Vfs_plugin::write(Libc::File_descriptor *fd, const void *buf,
                                ::size_t count)
{
	typedef Vfs::File_io_service::Write_result Result;

	Vfs::Vfs_handle *handle = vfs_handle(fd);

	Vfs::file_size out_count  = 0;
	Result         out_result = Result::WRITE_OK;

	if (fd->flags & O_NONBLOCK) {

		try {
			out_result = VFS_THREAD_SAFE(handle->fs().write(handle, (char const *)buf, count, out_count));

			/* wake up threads blocking for 'queue_*()' or 'write()' */
			Libc::resume_all();

		} catch (Vfs::File_io_service::Insufficient_buffer) { }

	} else {

		struct Check : Libc::Suspend_functor
		{
			bool             retry { false };

			Vfs::Vfs_handle *handle;
			void const      *buf;
			::size_t         count;
			Vfs::file_size  &out_count;
			Result          &out_result;

			Check(Vfs::Vfs_handle *handle, void const *buf,
			      ::size_t count, Vfs::file_size &out_count,
			      Result &out_result)
			: handle(handle), buf(buf), count(count), out_count(out_count),
			  out_result(out_result)
			{ }

			bool suspend() override
			{
				try {
					out_result = VFS_THREAD_SAFE(handle->fs().write(handle, (char const *)buf,
						                                              count, out_count));
					retry = false;
				} catch (Vfs::File_io_service::Insufficient_buffer) {
					retry = true;
				}

				return retry;
			}
		} check(handle, buf, count, out_count, out_result);

		do {
			Libc::suspend(check);
		} while (check.retry);
	}

	/* wake up threads blocking for 'queue_*()' or 'write()' */
	Libc::resume_all();

	switch (out_result) {
	case Result::WRITE_ERR_AGAIN:       return Errno(EAGAIN);
	case Result::WRITE_ERR_WOULD_BLOCK: return Errno(EWOULDBLOCK);
	case Result::WRITE_ERR_INVALID:     return Errno(EINVAL);
	case Result::WRITE_ERR_IO:          return Errno(EIO);
	case Result::WRITE_ERR_INTERRUPT:   return Errno(EINTR);
	case Result::WRITE_OK:              break;
	}

	VFS_THREAD_SAFE(handle->advance_seek(out_count));

	return out_count;
}


ssize_t Libc::Vfs_plugin::read(Libc::File_descriptor *fd, void *buf,
                               ::size_t count)
{
	Libc::dispatch_pending_io_signals();

	typedef Vfs::File_io_service::Read_result Result;

	Vfs::Vfs_handle *handle = vfs_handle(fd);

	if (fd->flags & O_DIRECTORY)
		return Errno(EISDIR);

	if (fd->flags & O_NONBLOCK && !Libc::read_ready(fd))
		return Errno(EAGAIN);

	{
		struct Check : Libc::Suspend_functor
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
			Libc::suspend(check);
		} while (check.retry);
	}

	Vfs::file_size out_count = 0;
	Result         out_result;

	{
		struct Check : Libc::Suspend_functor
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
			Libc::suspend(check);
		} while (check.retry);
	}

	/* wake up threads blocking for 'queue_*()' or 'write()' */
	Libc::resume_all();

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


ssize_t Libc::Vfs_plugin::getdirentries(Libc::File_descriptor *fd, char *buf,
                                        ::size_t nbytes, ::off_t *basep)
{
	if (nbytes < sizeof(struct dirent)) {
		Genode::error("getdirentries: buffer too small");
		return -1;
	}

	typedef Vfs::File_io_service::Read_result Result;

	Vfs::Vfs_handle *handle = vfs_handle(fd);

	typedef Vfs::Directory_service::Dirent Dirent;

	Dirent dirent_out;

	{
		struct Check : Libc::Suspend_functor
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
			Libc::suspend(check);
		} while (check.retry);
	}

	Result         out_result;
	Vfs::file_size out_count;

	{
		struct Check : Libc::Suspend_functor
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

				/* suspend me if read is still queued */

				retry = (out_result == Result::READ_QUEUED);

				return retry;
			}
		} check(handle, dirent_out, out_count, out_result);

		do {
			Libc::suspend(check);
		} while (check.retry);
	}

	/* wake up threads blocking for 'queue_*()' or 'write()' */
	Libc::resume_all();

	if ((out_result != Result::READ_OK) ||
	    (out_count < sizeof(Dirent))) {
		return 0;
	}

	/*
	 * Convert dirent structure from VFS to libc
	 */

	struct dirent *dirent = (struct dirent *)buf;
	Genode::memset(dirent, 0, sizeof(struct dirent));

	switch (dirent_out.type) {
	case Vfs::Directory_service::DIRENT_TYPE_DIRECTORY: dirent->d_type = DT_DIR;  break;
	case Vfs::Directory_service::DIRENT_TYPE_FILE:      dirent->d_type = DT_REG;  break;
	case Vfs::Directory_service::DIRENT_TYPE_SYMLINK:   dirent->d_type = DT_LNK;  break;
	case Vfs::Directory_service::DIRENT_TYPE_FIFO:      dirent->d_type = DT_FIFO; break;
	case Vfs::Directory_service::DIRENT_TYPE_CHARDEV:   dirent->d_type = DT_CHR;  break;
	case Vfs::Directory_service::DIRENT_TYPE_BLOCKDEV:  dirent->d_type = DT_BLK;  break;
	case Vfs::Directory_service::DIRENT_TYPE_END:                                 return 0;
	}

	dirent->d_fileno = dirent_out.fileno;
	dirent->d_reclen = sizeof(struct dirent);

	Genode::strncpy(dirent->d_name, dirent_out.name, sizeof(dirent->d_name));

	dirent->d_namlen = Genode::strlen(dirent->d_name);

	/*
	 * Keep track of VFS seek pointer and user-supplied basep.
	 */
	VFS_THREAD_SAFE(handle->advance_seek(sizeof(Vfs::Directory_service::Dirent)));

	*basep += sizeof(struct dirent);

	return sizeof(struct dirent);
}


int Libc::Vfs_plugin::ioctl(Libc::File_descriptor *fd, int request, char *argp)
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
		opcode = Opcode::IOCTL_OP_TIOCGWINSZ;
		break;

	case TIOCGETA:
		{
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

			return 0;
		}

		break;

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
		Genode::warning("unsupported ioctl (request=", Genode::Hex(request), ")");
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
			::winsize *winsize = (::winsize *)arg;
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


::off_t Libc::Vfs_plugin::lseek(Libc::File_descriptor *fd, ::off_t offset, int whence)
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


int Libc::Vfs_plugin::ftruncate(Libc::File_descriptor *fd, ::off_t length)
{
	Vfs::Vfs_handle *handle = vfs_handle(fd);
	_vfs_sync(handle);

	typedef Vfs::File_io_service::Ftruncate_result Result;

	switch (VFS_THREAD_SAFE(handle->fs().ftruncate(handle, length))) {
	case Result::FTRUNCATE_ERR_NO_PERM:   errno = EPERM;  return -1;
	case Result::FTRUNCATE_ERR_INTERRUPT: errno = EINTR;  return -1;
	case Result::FTRUNCATE_ERR_NO_SPACE:  errno = ENOSPC; return -1;
	case Result::FTRUNCATE_OK:                            break;
	}
	return 0;
}


int Libc::Vfs_plugin::fcntl(Libc::File_descriptor *fd, int cmd, long arg)
{
	switch (cmd) {
	case F_DUPFD:
		{
			/*
			 * Allocate free file descriptor locally.
			 */
			Libc::File_descriptor *new_fd =
				Libc::file_descriptor_allocator()->alloc(this, 0);
			if (!new_fd) return Errno(EMFILE);

			new_fd->path(fd->fd_path);

			/*
			 * Use new allocated number as name of file descriptor
			 * duplicate.
			 */
			if (dup2(fd, new_fd) == -1) {
				Genode::error("Plugin::fcntl: dup2 unexpectedly failed");
				return Errno(EINVAL);
			}

			return new_fd->libc_fd;
		}
	case F_GETFD: return fd->cloexec ? FD_CLOEXEC : 0;
	case F_SETFD: fd->cloexec = arg == FD_CLOEXEC;  return 0;

	case F_GETFL: return fd->flags;
	case F_SETFL: fd->flags = arg; return 0;

	default:
		break;
	}

	Genode::error("fcntl(): command ", cmd, " not supported - vfs");
	return Errno(EINVAL);
}


int Libc::Vfs_plugin::fsync(Libc::File_descriptor *fd)
{
	Vfs::Vfs_handle *handle = vfs_handle(fd);
	return _vfs_sync(handle);
}


int Libc::Vfs_plugin::symlink(const char *oldpath, const char *newpath)
{
	typedef Vfs::Directory_service::Openlink_result Openlink_result;

	Vfs::Vfs_handle *handle { 0 };

	Openlink_result openlink_result =
		VFS_THREAD_SAFE(_root_dir.openlink(newpath, true, &handle, _alloc));

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

	struct Check : Libc::Suspend_functor
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
		Libc::suspend(check);
	} while (check.retry);

	/* wake up threads blocking for 'queue_*()' or 'write()' */
	Libc::resume_all();

	_vfs_sync(handle);
	VFS_THREAD_SAFE(handle->close());

	if (out_count != count)
		return Errno(ENAMETOOLONG);

	return 0;
}


ssize_t Libc::Vfs_plugin::readlink(const char *path, char *buf, ::size_t buf_size)
{
	Vfs::Vfs_handle *symlink_handle { 0 };

	Vfs::Directory_service::Openlink_result openlink_result =
		VFS_THREAD_SAFE(_root_dir.openlink(path, false, &symlink_handle, _alloc));

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

	{
		struct Check : Libc::Suspend_functor
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
			Libc::suspend(check);
		} while (check.retry);
	}

	typedef Vfs::File_io_service::Read_result Result;

	Result out_result;
	Vfs::file_size out_len = 0;

	{
		struct Check : Libc::Suspend_functor
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
			Libc::suspend(check);
		} while (check.retry);
	}

	/* wake up threads blocking for 'queue_*()' or 'write()' */
	Libc::resume_all();

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

	switch (VFS_THREAD_SAFE(_root_dir.unlink(path))) {
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

	if (VFS_THREAD_SAFE(_root_dir.leaf_path(to_path))) {

		if (VFS_THREAD_SAFE(_root_dir.directory(to_path))) {
			if (!VFS_THREAD_SAFE(_root_dir.directory(from_path))) {
				errno = EISDIR; return -1;
			}

			if (VFS_THREAD_SAFE(_root_dir.num_dirent(to_path))) {
				errno = ENOTEMPTY; return -1;
			}

		} else {
			if (VFS_THREAD_SAFE(_root_dir.directory(from_path))) {
				errno = ENOTDIR; return -1;
			}
		}
	}

	switch (VFS_THREAD_SAFE(_root_dir.rename(from_path, to_path))) {
	case Result::RENAME_ERR_NO_ENTRY: errno = ENOENT; return -1;
	case Result::RENAME_ERR_CROSS_FS: errno = EXDEV;  return -1;
	case Result::RENAME_ERR_NO_PERM:  errno = EPERM;  return -1;
	case Result::RENAME_OK:                           break;
	}
	return 0;
}


void *Libc::Vfs_plugin::mmap(void *addr_in, ::size_t length, int prot, int flags,
                             Libc::File_descriptor *fd, ::off_t offset)
{
	if (prot != PROT_READ) {
		Genode::error("mmap for prot=", Genode::Hex(prot), " not supported");
		errno = EACCES;
		return (void *)-1;
	}

	if (addr_in != 0) {
		Genode::error("mmap for predefined address not supported");
		errno = EINVAL;
		return (void *)-1;
	}

	/*
	 * XXX attempt to obtain memory mapping via
	 *     'Vfs::Directory_service::dataspace'.
	 */

	void *addr = Libc::mem_alloc()->alloc(length, PAGE_SHIFT);
	if (addr == (void *)-1) {
		errno = ENOMEM;
		return (void *)-1;
	}

	if (::pread(fd->libc_fd, addr, length, offset) < 0) {
		Genode::error("mmap could not obtain file content");
		::munmap(addr, length);
		errno = EACCES;
		return (void *)-1;
	}

	return addr;
}


int Libc::Vfs_plugin::munmap(void *addr, ::size_t)
{
	Libc::mem_alloc()->free(addr);
	return 0;
}


bool Libc::Vfs_plugin::supports_select(int nfds,
                                       fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
                                       struct timeval *timeout)
{
	/* return true if any file descriptor (which is set) belongs to the VFS */
	for (int fd = 0; fd < nfds; ++fd) {

		if (FD_ISSET(fd, readfds) || FD_ISSET(fd, writefds) || FD_ISSET(fd, exceptfds)) {

			Libc::File_descriptor *fdo =
				Libc::file_descriptor_allocator()->find_by_libc_fd(fd);

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

		Libc::File_descriptor *fdo =
			Libc::file_descriptor_allocator()->find_by_libc_fd(fd);

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
				Libc::notify_read_ready(handle);
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
