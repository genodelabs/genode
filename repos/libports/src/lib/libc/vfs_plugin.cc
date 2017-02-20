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
	enum { FS_BLOCK_SIZE = 1024 };

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


namespace Libc {

	void libc_config_init(Genode::Xml_node node)
	{
		static Genode::Xml_node config = node;
		_config_node = &config;
	}

	Genode::Xml_node config() __attribute__((weak));
	Genode::Xml_node config()
	{
		return _config_node->sub_node("libc");
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
}

int Libc::Vfs_plugin::access(const char *path, int amode)
{
	if (_root_dir.leaf_path(path))
		return 0;

	errno = ENOENT;
	return -1;
}


Libc::File_descriptor *Libc::Vfs_plugin::open(char const *path, int flags,
                                              int libc_fd)
{
	typedef Vfs::Directory_service::Open_result Result;

	Vfs::Vfs_handle *handle = 0;

	while (handle == 0) {

		switch (_root_dir.open(path, flags, &handle, _alloc)) {

		case Result::OPEN_OK:
			break;

		case Result::OPEN_ERR_UNACCESSIBLE:
			{
				if (!(flags & O_CREAT)) {
					errno = ENOENT;
					return 0;
				}

				/* O_CREAT is set, so try to create the file */
				switch (_root_dir.open(path, flags | O_EXCL, &handle, _alloc)) {

				case Result::OPEN_OK:
					break;

				case Result::OPEN_ERR_EXISTS:

					/* file has been created by someone else in the meantime */
					break;

				case Result::OPEN_ERR_NO_PERM:       errno = EPERM;        return 0;
				case Result::OPEN_ERR_UNACCESSIBLE:  errno = ENOENT;       return 0;
				case Result::OPEN_ERR_NAME_TOO_LONG: errno = ENAMETOOLONG; return 0;
				case Result::OPEN_ERR_NO_SPACE:      errno = ENOSPC;       return 0;
				}
			}
			break;

		case Result::OPEN_ERR_NO_PERM:       errno = EPERM;        return 0;
		case Result::OPEN_ERR_EXISTS:        errno = EEXIST;       return 0;
		case Result::OPEN_ERR_NAME_TOO_LONG: errno = ENAMETOOLONG; return 0;
		case Result::OPEN_ERR_NO_SPACE:      errno = ENOSPC;       return 0;
		}
	}

	/* the file was successfully opened */

	Libc::File_descriptor *fd =
		Libc::file_descriptor_allocator()->alloc(this, vfs_context(handle), libc_fd);

	fd->status = flags;

	if ((flags & O_TRUNC) && (ftruncate(fd, 0) == -1)) {
		/* XXX leaking fd, missing errno */
		return 0;
	}

	return fd;
}


int Libc::Vfs_plugin::close(Libc::File_descriptor *fd)
{
	Vfs::Vfs_handle *handle = vfs_handle(fd);
	handle->ds().close(handle);
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
	typedef Vfs::Directory_service::Mkdir_result Result;

	switch (_root_dir.mkdir(path, mode)) {
	case Result::MKDIR_ERR_EXISTS:        errno = EEXIST;       return -1;
	case Result::MKDIR_ERR_NO_ENTRY:      errno = ENOENT;       return -1;
	case Result::MKDIR_ERR_NO_SPACE:      errno = ENOSPC;       return -1;
	case Result::MKDIR_ERR_NAME_TOO_LONG: errno = ENAMETOOLONG; return -1;
	case Result::MKDIR_ERR_NO_PERM:       errno = EPERM;        return -1;
	case Result::MKDIR_OK:                                      break;
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

	switch (_root_dir.stat(path, stat)) {
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

	Vfs::file_size out_count = 0;

	switch (handle->fs().write(handle, (char const *)buf, count, out_count)) {
	case Result::WRITE_ERR_AGAIN:       errno = EAGAIN;      return -1;
	case Result::WRITE_ERR_WOULD_BLOCK: errno = EWOULDBLOCK; return -1;
	case Result::WRITE_ERR_INVALID:     errno = EINVAL;      return -1;
	case Result::WRITE_ERR_IO:          errno = EIO;         return -1;
	case Result::WRITE_ERR_INTERRUPT:   errno = EINTR;       return -1;
	case Result::WRITE_OK:                                   break;
	}

	handle->advance_seek(out_count);

	return out_count;
}


typedef Vfs::File_io_service::Read_result Result;

struct Read_check : Libc::Suspend_functor {
	Vfs::Vfs_handle * handle;
	void            * buf;
	::size_t        * count;
	Vfs::file_size  * out_count;
	Result          * out_result;

	Read_check(Vfs::Vfs_handle * handle, void * buf, ::size_t * count,
	           Vfs::file_size  * out_count, Result * out_result)
	: handle(handle), buf(buf), count(count), out_count(out_count),
	  out_result(out_result)
	{ }
};

ssize_t Libc::Vfs_plugin::read(Libc::File_descriptor *fd, void *buf,
                               ::size_t count)
{
	Vfs::Vfs_handle *handle = vfs_handle(fd);

	Vfs::file_size out_count  = 0;
	Result         out_result = Result::READ_OK;

	while (!handle->fs().queue_read(handle, (char *)buf, count,
	                                out_result, out_count)) {
		struct Check : Read_check {
			Check(Vfs::Vfs_handle * handle, void * buf, ::size_t * count,
			      Vfs::file_size * out_count, Result * out_result)
			: Read_check (handle, buf, count, out_count, out_result) { }

			bool suspend() override {
				return !handle->fs().queue_read(handle, (char *)buf, *count,
				                                *out_result, *out_count); }
		} check ( handle, buf, &count, &out_count, &out_result);

		Libc::suspend(check);
	}

	while (out_result == Result::READ_QUEUED) {

		struct Check : Read_check {
			Check(Vfs::Vfs_handle * handle, void * buf, ::size_t * count,
			      Vfs::file_size * out_count, Result * out_result)
			: Read_check (handle, buf, count, out_count, out_result) { }

			bool suspend() override {
				*out_result = handle->fs().complete_read(handle, (char *)buf,
				                                         *count, *out_count);
				/* suspend me if read is still queued */
				return *out_result == Result::READ_QUEUED;
			}
		} check ( handle, buf, &count, &out_count, &out_result);

		Libc::suspend(check);
	}

	switch (out_result) {
	case Result::READ_ERR_AGAIN:       return Errno(EAGAIN);
	case Result::READ_ERR_WOULD_BLOCK: return Errno(EWOULDBLOCK);
	case Result::READ_ERR_INVALID:     return Errno(EINVAL);
	case Result::READ_ERR_IO:          return Errno(EIO);
	case Result::READ_ERR_INTERRUPT:   return Errno(EINTR);
	case Result::READ_OK:              break;

	case Result::READ_QUEUED: /* handled above, so never reached */ break;
	}

	handle->advance_seek(out_count);

	return out_count;
}


ssize_t Libc::Vfs_plugin::getdirentries(Libc::File_descriptor *fd, char *buf,
                                        ::size_t nbytes, ::off_t *basep)
{
	if (nbytes < sizeof(struct dirent)) {
		Genode::error("getdirentries: buffer too small");
		return -1;
	}

	typedef Vfs::Directory_service::Dirent_result Result;

	Vfs::Vfs_handle *handle = vfs_handle(fd);

	Vfs::Directory_service::Dirent dirent_out;
	Genode::memset(&dirent_out, 0, sizeof(dirent_out));

	unsigned const index = handle->seek() / sizeof(Vfs::Directory_service::Dirent);

	switch (handle->ds().dirent(fd->fd_path, index, dirent_out)) {
	case Result::DIRENT_ERR_INVALID_PATH: errno = ENOENT; return -1;
	case Result::DIRENT_ERR_NO_PERM:      errno = EACCES; return -1;
	case Result::DIRENT_OK:                               break;
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
	handle->advance_seek(sizeof(Vfs::Directory_service::Dirent));

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

	switch (handle->fs().ioctl(handle, opcode, arg, out)) {
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

			int64_t *disk_size = (int64_t*)arg;
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

	typedef Vfs::File_io_service::Ftruncate_result Result;

	switch (handle->fs().ftruncate(handle, length)) {
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
			new_fd->path(fd->fd_path);

			/*
			 * Use new allocated number as name of file descriptor
			 * duplicate.
			 */
			if (dup2(fd, new_fd) == -1) {
				Genode::error("Plugin::fcntl: dup2 unexpectedly failed");
				errno = EINVAL;
				return -1;
			}

			return new_fd->libc_fd;
		}
	case F_GETFD:                  return fd->flags;
	case F_SETFD: fd->flags = arg; return 0;
	case F_GETFL:                  return fd->status;

	default:
		break;
	}

	Genode::error("fcntl(): command ", cmd, " not supported - vfs");
	errno = EINVAL;
	return -1;
}


int Libc::Vfs_plugin::fsync(Libc::File_descriptor *fd)
{
	_root_dir.sync(fd->fd_path);
	return 0;
}


int Libc::Vfs_plugin::symlink(const char *oldpath, const char *newpath)
{
	typedef Vfs::Directory_service::Symlink_result Result;

	switch (_root_dir.symlink(oldpath, newpath)) {
	case Result::SYMLINK_ERR_EXISTS:        errno = EEXIST;       return -1;
	case Result::SYMLINK_ERR_NO_ENTRY:      errno = ENOENT;       return -1;
	case Result::SYMLINK_ERR_NAME_TOO_LONG: errno = ENAMETOOLONG; return -1;
	case Result::SYMLINK_ERR_NO_PERM:       errno = ENOSYS;       return -1;
	case Result::SYMLINK_ERR_NO_SPACE:      errno = ENOSPC;       return -1;
	case Result::SYMLINK_OK:                                      break;
	}
	return 0;
}


ssize_t Libc::Vfs_plugin::readlink(const char *path, char *buf, ::size_t buf_size)
{
	typedef Vfs::Directory_service::Readlink_result Result;

	Vfs::file_size out_len = 0;

	switch (_root_dir.readlink(path, buf, buf_size, out_len)) {
	case Result::READLINK_ERR_NO_ENTRY: errno = ENOENT; return -1;
	case Result::READLINK_ERR_NO_PERM:  errno = EACCES; return -1;
	case Result::READLINK_OK:                           break;
	};

	return out_len;
}


int Libc::Vfs_plugin::rmdir(char const *path)
{
	return unlink(path);
}


int Libc::Vfs_plugin::unlink(char const *path)
{
	typedef Vfs::Directory_service::Unlink_result Result;

	switch (_root_dir.unlink(path)) {
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

	if (_root_dir.leaf_path(to_path)) {

		if (_root_dir.directory(to_path)) {
			if (!_root_dir.directory(from_path)) {
				errno = EISDIR; return -1;
			}

			if (_root_dir.num_dirent(to_path)) {
				errno = ENOTEMPTY; return -1;
			}

		} else {
			if (_root_dir.directory(from_path)) {
				errno = ENOTDIR; return -1;
			}
		}
	}

	switch (_root_dir.rename(from_path, to_path)) {
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
			if (handle->fs().read_ready(handle)) {
				FD_SET(fd, readfds);
				++nready;
			} else {
				struct Check : Libc::Suspend_functor {
					Vfs::Vfs_handle * handle;
					Check(Vfs::Vfs_handle * handle) : handle (handle) { }
					bool suspend() override {
						return !handle->fs().notify_read_ready(handle); }
				} check ( handle );

				while (!handle->fs().notify_read_ready(handle))
					Libc::suspend(check);
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

namespace Libc {

	bool read_ready(Libc::File_descriptor *fd)
	{
		Vfs::Vfs_handle *handle = vfs_handle(fd);
		if (!handle) return false;

		handle->fs().notify_read_ready(handle);

		return handle->fs().read_ready(handle);
	}

}
