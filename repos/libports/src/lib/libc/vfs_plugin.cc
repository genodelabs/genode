/*
 * \brief   Libc plugin for using a process-local virtual file system
 * \author  Norman Feske
 * \date    2014-04-09
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/env.h>
#include <base/printf.h>
#include <vfs/dir_file_system.h>
#include <os/config.h>

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
#include <libc-plugin/fd_alloc.h>

/* libc-internal includes */
#include <libc_mem_alloc.h>


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


namespace Libc {

	Genode::Xml_node config() __attribute__((weak));
	Genode::Xml_node config()
	{
		return Genode::config()->xml_node().sub_node("libc");
	}


	Genode::Xml_node vfs_config() __attribute__((weak));
	Genode::Xml_node vfs_config()
	{
		return Libc::config().sub_node("vfs");
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


	char const *initial_cwd() __attribute__((weak));
	char const *initial_cwd()
	{
		static Config_attr initial_cwd("cwd", "/");
		return initial_cwd.string();
	}


	char const *config_stdin() __attribute__((weak));
	char const *config_stdin()
	{
		static Config_attr stdin("stdin", "");
		return stdin.string();
	}


	char const *config_stdout() __attribute__((weak));
	char const *config_stdout()
	{
		static Config_attr stdout("stdout", "");
		return stdout.string();
	}


	char const *config_stderr() __attribute__((weak));
	char const *config_stderr()
	{
		static Config_attr stderr("stderr", "");
		return stderr.string();
	}

	char const *config_rtc() __attribute__((weak));
	char const *config_rtc()
	{
		static Config_attr rtc("rtc", "");
		return rtc.string();
	}
}


namespace Libc { class Vfs_plugin; }


class Libc::Vfs_plugin : public Libc::Plugin
{
	private:

		Vfs::Dir_file_system _root_dir;

		Genode::Xml_node _vfs_config()
		{
			try {
				return vfs_config();
			} catch (...) {
				PINF("no VFS configured");
				return Genode::Xml_node("<vfs/>");
			}
		}

		void _open_stdio(int libc_fd, char const *path, unsigned flags)
		{
			struct stat out_stat;
			if (::strlen(path) == 0 || stat(path, &out_stat) != 0)
				return;

			Libc::File_descriptor *fd = open(path, flags, libc_fd);
			if (fd->libc_fd != libc_fd) {
				PERR("could not allocate fd %d for %s, got fd %d",
				     libc_fd, path, fd->libc_fd);
				close(fd);
				return;
			}

			/*
			 * We need to manually register the path. Normally this is done
			 * by '_open'. But we call the local 'open' function directly
			 * because we want to explicitly specify the libc fd ID.
			 *
			 * We have to allocate the path from the libc (done via 'strdup')
			 * such that the path can be freed when an stdio fd is closed.
			 */
			fd->fd_path = strdup(path);
		}

	public:

		/**
		 * Constructor
		 */
		Vfs_plugin() : _root_dir(_vfs_config(), Vfs::global_file_system_factory())
		{
			chdir(initial_cwd());

			_open_stdio(0, config_stdin(),  O_RDONLY);
			_open_stdio(1, config_stdout(), O_WRONLY);
			_open_stdio(2, config_stderr(), O_WRONLY);
		}

		~Vfs_plugin() { }

		bool supports_access(const char *, int)              override { return true; }
		bool supports_mkdir(const char *, mode_t)            override { return true; }
		bool supports_open(const char *, int)                override { return true; }
		bool supports_readlink(const char *, char *, size_t) override { return true; }
		bool supports_rename(const char *, const char *)     override { return true; }
		bool supports_rmdir(const char *)                    override { return true; }
		bool supports_stat(const char *)                     override { return true; }
		bool supports_symlink(const char *, const char *)    override { return true; }
		bool supports_unlink(const char *)                   override { return true; }
		bool supports_mmap()                                 override { return true; }

		Libc::File_descriptor *open(const char *, int, int libc_fd);

		Libc::File_descriptor *open(const char *path, int flags) override
		{
			return open(path, flags, Libc::ANY_FD);
		}

		int     access(char const *, int) override;
		int     close(Libc::File_descriptor *) override;
		int     dup2(Libc::File_descriptor *, Libc::File_descriptor *) override;
		int     fcntl(Libc::File_descriptor *, int, long) override;
		int     fstat(Libc::File_descriptor *, struct stat *) override;
		int     fstatfs(Libc::File_descriptor *, struct statfs *) override;
		int     fsync(Libc::File_descriptor *fd) override;
		int     ftruncate(Libc::File_descriptor *, ::off_t) override;
		ssize_t getdirentries(Libc::File_descriptor *, char *, ::size_t , ::off_t *) override;
		int     ioctl(Libc::File_descriptor *, int , char *) override;
		::off_t lseek(Libc::File_descriptor *fd, ::off_t offset, int whence) override;
		int     mkdir(const char *, mode_t) override;
		ssize_t read(Libc::File_descriptor *, void *, ::size_t) override;
		ssize_t readlink(const char *, char *, size_t) override;
		int     rename(const char *, const char *) override;
		int     rmdir(const char *) override;
		int     stat(const char *, struct stat *) override;
		int     symlink(const char *, const char *) override;
		int     unlink(const char *) override;
		ssize_t write(Libc::File_descriptor *, const void *, ::size_t ) override;
		void   *mmap(void *, ::size_t, int, int, Libc::File_descriptor *, ::off_t) override;
		int     munmap(void *, ::size_t) override;
};


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

		switch (_root_dir.open(path, flags, &handle)) {

		case Result::OPEN_OK:
			break;

		case Result::OPEN_ERR_UNACCESSIBLE:
			{
				if (!(flags & O_CREAT)) {
					errno = ENOENT;
					return 0;
				}

				/* O_CREAT is set, so try to create the file */
				switch (_root_dir.open(path, flags | O_EXCL, &handle)) {

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
	Genode::destroy(Genode::env()->heap(), vfs_handle(fd));
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


int Libc::Vfs_plugin::fstatfs(Libc::File_descriptor *, struct statfs *buf)
{
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


ssize_t Libc::Vfs_plugin::read(Libc::File_descriptor *fd, void *buf,
                               ::size_t count)
{
	typedef Vfs::File_io_service::Read_result Result;

	Vfs::Vfs_handle *handle = vfs_handle(fd);

	Vfs::file_size out_count = 0;

	switch (handle->fs().read(handle, (char *)buf, count, out_count)) {
	case Result::READ_ERR_AGAIN:       errno = EAGAIN;      return -1;
	case Result::READ_ERR_WOULD_BLOCK: errno = EWOULDBLOCK; return -1;
	case Result::READ_ERR_INVALID:     errno = EINVAL;      return -1;
	case Result::READ_ERR_IO:          errno = EIO;         return -1;
	case Result::READ_ERR_INTERRUPT:   errno = EINTR;       return -1;
	case Result::READ_OK:                                   break;
	}

	handle->advance_seek(out_count);

	return out_count;
}


ssize_t Libc::Vfs_plugin::getdirentries(Libc::File_descriptor *fd, char *buf,
                                        ::size_t nbytes, ::off_t *basep)
{
	if (nbytes < sizeof(struct dirent)) {
		PERR("getdirentries: buffer too small");
		return -1;
	}

	typedef Vfs::Directory_service::Dirent_result Result;

	Vfs::Vfs_handle *handle = vfs_handle(fd);

	Vfs::Directory_service::Dirent dirent_out;
	Genode::memset(&dirent_out, 0, sizeof(dirent_out));

	unsigned const index = handle->seek() / sizeof(Vfs::Directory_service::Dirent);

	switch (handle->ds().dirent(fd->fd_path, index, dirent_out)) {
	case Result::DIRENT_ERR_INVALID_PATH: /* XXX errno */ return -1;
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
		PWRN("unsupported ioctl (request=0x%x)", request);
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
				PERR("Plugin::fcntl: dup2 unexpectedly failed");
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

	PERR("fcntl(): command %d not supported - vfs", cmd);
	errno = EINVAL;
	return -1;
}


int Libc::Vfs_plugin::fsync(Libc::File_descriptor *fd)
{
	_root_dir.sync();
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


ssize_t Libc::Vfs_plugin::readlink(const char *path, char *buf, size_t buf_size)
{
	typedef Vfs::Directory_service::Readlink_result Result;

	Vfs::file_size out_len = 0;

	switch (_root_dir.readlink(path, buf, buf_size, out_len)) {
	case Result::READLINK_ERR_NO_ENTRY: errno = ENOENT; return -1;
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
	case Result::UNLINK_ERR_NO_ENTRY: errno = ENOENT; return -1;
	case Result::UNLINK_ERR_NO_PERM:  errno = EPERM;  return -1;
	case Result::UNLINK_OK:                           break;
	}
	return 0;
}


int Libc::Vfs_plugin::rename(char const *from_path, char const *to_path)
{
	typedef Vfs::Directory_service::Rename_result Result;

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
		PERR("mmap for prot=%x not supported", prot);
		errno = EACCES;
		return (void *)-1;
	}

	if (addr_in != 0) {
		PERR("mmap for predefined address not supported");
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
		PERR("mmap could not obtain file content");
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


void __attribute__((constructor)) init_libc_vfs(void)
{
	static Libc::Vfs_plugin plugin;
}
