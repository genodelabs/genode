/*
 * \brief   FFAT libc plugin
 * \author  Christian Prochaska
 * \date    2011-05-27
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/env.h>
#include <base/log.h>
#include <os/path.h>

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

/* ffat includes */
namespace Ffat { extern "C" {
#include <ffat/ff.h>
} }

/*
 * These macros are defined in later versions of the FatFs lib, but not in the
 * one currently used for Genode.
 */
#define f_size(fp) ((fp)->fsize)
#define f_tell(fp) ((fp)->fptr)

static bool const verbose = false;


namespace {


class Plugin_context : public Libc::Plugin_context
{
	private:

		char *_filename; /* needed for fstat() */
		int   _fd_flags;
		int   _status_flags;

	public:

		Plugin_context(const char *filename)
		: _fd_flags(0), _status_flags(0)
		{
			_filename = (char*)malloc(::strlen(filename) + 1);
			::strcpy(_filename, filename);
		}

		virtual ~Plugin_context()
		{
			::free(_filename);
		}

		const char *filename() { return _filename; }

		/**
		 * Set/get file descriptor flags
		 */
		void fd_flags(int flags) { _fd_flags = flags; }
		int fd_flags() { return _fd_flags; }

		/**
		 * Set/get file status status flags
		 */
		void status_flags(int flags) { _status_flags = flags; }
		int status_flags() { return _status_flags; }
};


static inline Plugin_context *context(Libc::File_descriptor *fd)
{
	return fd->context ? static_cast<Plugin_context *>(fd->context) : 0;
}


class File_plugin_context : public Plugin_context
{
	private:

		Ffat::FIL _ffat_file;

	public:

		/**
		 * Constructor
		 *
		 * \param filename
		 * \param ffat_file file object used for interacting with the
		 *                  file API of ffat
		 */
		File_plugin_context(const char *filename, Ffat::FIL ffat_file) :
			Plugin_context(filename), _ffat_file(ffat_file) { }

		Ffat::FIL *ffat_file() { return &_ffat_file; }
};


class Directory_plugin_context : public Plugin_context
{
	private:

		Ffat::DIR _ffat_dir;

	public:

		/**
		 * Constructor
		 *
		 * \param filename
		 * \param ffat_dir dir object used for interacting with the
		 *                 file API of ffat
		 */
		Directory_plugin_context(const char *filename, Ffat::DIR ffat_dir) :
			Plugin_context(filename), _ffat_dir(ffat_dir) { }

		Ffat::DIR *ffat_dir() { return &_ffat_dir; }
};


class Plugin : public Libc::Plugin
{
	private:

		Ffat::FATFS _fatfs;

		Ffat::FIL *_get_ffat_file(Libc::File_descriptor *fd)
		{
			File_plugin_context *file_plugin_context =
				dynamic_cast<File_plugin_context*>(context(fd));
			if (!file_plugin_context) {
				return 0;
			}
			return file_plugin_context->ffat_file();
		}

		Ffat::DIR *_get_ffat_dir(Libc::File_descriptor *fd)
		{
			Directory_plugin_context *directory_plugin_context =
				dynamic_cast<Directory_plugin_context*>(context(fd));
			if (!directory_plugin_context) {
				return 0;
			}
			return directory_plugin_context->ffat_dir();
		}

		/*
		 * Override libc_vfs
		 */
		enum { PLUGIN_PRIORITY = 1 };

	public:

		/**
		 * Constructor
		 */
		Plugin() : Libc::Plugin(PLUGIN_PRIORITY)
		{
			/* mount the file system */
			if (verbose)
				Genode::log(__func__, ": mounting device ...");

			if (f_mount(0, &_fatfs) != Ffat::FR_OK) {
				Genode::error("mount failed");
			}
		}

		~Plugin()
		{
			/* unmount the file system */
			Ffat::f_mount(0, NULL);
		}

		/*
		 * TODO: decide if the file named <path> shall be handled by this plugin
		 */

		bool supports_mkdir(const char *path, mode_t) override
		{
			if (verbose)
				Genode::log(__func__, ": path=", path);
			return true;
		}

		bool supports_open(const char *pathname, int flags) override
		{
			if (verbose)
				Genode::log(__func__, ": pathname=", pathname);
			return true;
		}

		bool supports_rename(const char *oldpath, const char *newpath) override
		{
			if (verbose)
				Genode::log(__func__, ": oldpath=", oldpath, ", newpath=", newpath);
			return true;
		}

		bool supports_rmdir(const char *path) override
		{
			if (verbose)
				Genode::log(__func__, ": path=", path);
			return true;
		}

		bool supports_stat(const char *path) override
		{
			if (verbose)
				Genode::log(__func__, ": path=", path);
			return true;
		}

		bool supports_unlink(const char *path) override
		{
			if (verbose)
				Genode::log(__func__, ": path=", path);
			return true;
		}

		bool supports_symlink(const char *, const char *) override
		{
			/*
			 * Even though FFAT does not support symlinks, we still want
			 * to capture calls of 'symlink' to return ENOSYS, which is
			 * checked in the file-system test.
			 */
			return true;
		}

		int close(Libc::File_descriptor *fd) override
		{
			using namespace Ffat;

			FIL *ffat_file = _get_ffat_file(fd);

			if (!ffat_file){
				/* directory */
				Genode::destroy(Genode::env()->heap(), context(fd));
				Libc::file_descriptor_allocator()->free(fd);
				return 0;
			}

			FRESULT res = f_close(ffat_file);

			Genode::destroy(Genode::env()->heap(), context(fd));
			Libc::file_descriptor_allocator()->free(fd);

			switch(res) {
				case FR_OK:
					return 0;
				case FR_DISK_ERR:
				case FR_INT_ERR:
				case FR_NOT_READY:
				case FR_INVALID_OBJECT:
					errno = EIO;
					return -1;
				default:
					/* not supposed to occur according to the libffat documentation */
					Genode::error("f_close() returned an unexpected error code");
					return -1;
			}
		}

		int fcntl(Libc::File_descriptor *fd, int cmd, long arg) override
		{
			switch (cmd) {
				case F_GETFD: return context(fd)->fd_flags();
				case F_SETFD: context(fd)->fd_flags(arg); return 0;
				case F_GETFL: return context(fd)->status_flags();
				default: Genode::error("fcntl(): command ", cmd, " not supported", cmd); return -1;
			}
		}

		int fstat(Libc::File_descriptor *fd, struct stat *buf) override
		{
			return stat(context(fd)->filename(), buf);
		}

		int fstatfs(Libc::File_descriptor *, struct statfs *buf) override
		{
			/* libc's opendir() fails if _fstatfs() returns -1, so we return 0 here */
			if (verbose)
				Genode::warning("_fstatfs() called - not yet implemented");
			return 0;
		}

		int fsync(Libc::File_descriptor *fd) override
		{
			using namespace Ffat;

			FRESULT res = f_sync(_get_ffat_file(fd));

			switch(res) {
				case FR_OK:
					return 0;
				case FR_DISK_ERR:
				case FR_INT_ERR:
				case FR_NOT_READY:
				case FR_INVALID_OBJECT:
					errno = EIO;
					return -1;
				default:
					/* not supposed to occur according to the libffat documentation */
					Genode::error("f_sync() returned an unexpected error code");
					return -1;
			}
		}

		int ftruncate(Libc::File_descriptor *fd, ::off_t length) override
		{
			using namespace Ffat;

			/* 'f_truncate()' truncates to the current seek pointer */

			if (lseek(fd, length, SEEK_SET) == -1)
				return -1;

			FRESULT res = f_truncate(_get_ffat_file(fd));

			switch(res) {
				case FR_OK:
					return 0;
				case FR_DISK_ERR:
				case FR_INT_ERR:
				case FR_NOT_READY:
				case FR_INVALID_OBJECT:
					errno = EIO;
					return -1;
				default:
					/* not supposed to occur according to the libffat documentation */
					Genode::error("f_truncate() returned an unexpected error code");
					return -1;
			}
		}

		ssize_t getdirentries(Libc::File_descriptor *fd, char *buf,
		                      ::size_t nbytes, ::off_t *basep) override
		{
			using namespace Ffat;

			if (nbytes < sizeof(struct dirent)) {
				Genode::error(__func__, ": buf too small");
				errno = ENOMEM;
				return -1;
			}

			struct dirent *dirent = (struct dirent *)buf;
			::memset(dirent, 0, sizeof(struct dirent));

			FILINFO ffat_file_info;
			ffat_file_info.lfname = dirent->d_name;
			ffat_file_info.lfsize = sizeof(dirent->d_name);

			FRESULT res = f_readdir(_get_ffat_dir(fd), &ffat_file_info);
			switch(res) {
				case FR_OK:
					break;
				case FR_DISK_ERR:
				case FR_INT_ERR:
				case FR_NOT_READY:
				case FR_INVALID_OBJECT:
					errno = EIO;
					return -1;
				default:
					/* not supposed to occur according to the libffat documentation */
					Genode::error("f_readdir() returned an unexpected error code");
					return -1;
			}

			if (ffat_file_info.fname[0] == 0) { /* no (more) entries */
				if (verbose)
					Genode::log(__func__, ": no more dir entries");
				/* TODO: reset the f_readdir() index? */
				return 0;
			}

			dirent->d_ino = 1; /* libc's readdir() wants an inode number */

			if ((ffat_file_info.fattrib & AM_DIR) == AM_DIR)
				dirent->d_type = DT_DIR;
			else
				dirent->d_type = DT_REG;

			dirent->d_reclen = sizeof(struct dirent);

			if (dirent->d_name[0] == 0) /* use short file name */
				::strncpy(dirent->d_name, ffat_file_info.fname,
						  sizeof(dirent->d_name));

			dirent->d_namlen = ::strlen(dirent->d_name);

			if (verbose)
				Genode::log("found dir entry ", Genode::Cstring(dirent->d_name));

			*basep += sizeof(struct dirent);
			return sizeof(struct dirent);
		}

		::off_t lseek(Libc::File_descriptor *fd, ::off_t offset, int whence) override
		{
			using namespace Ffat;

			switch(whence) {
				case SEEK_CUR:
					offset += f_tell(_get_ffat_file(fd));
					break;
				case SEEK_END:
					offset += f_size(_get_ffat_file(fd));
					break;
				default:
					break;
			}

			FRESULT res = f_lseek(_get_ffat_file(fd), offset);

			switch(res) {
				case FR_OK:
					/* according to the FatFs documentation this can happen */
					if ((off_t)f_tell(_get_ffat_file(fd)) != offset) {
						errno = EINVAL;
						return -1;
					}
					return offset;
				case FR_DISK_ERR:
				case FR_INT_ERR:
				case FR_NOT_READY:
				case FR_INVALID_OBJECT:
					errno = EIO;
					return -1;
				default:
					/* not supposed to occur according to the libffat documentation */
					Genode::error("f_lseek() returned an unexpected error code");
					return -1;
			}
		}

		int mkdir(const char *path, mode_t mode) override
		{
			using namespace Ffat;

			FRESULT res = f_mkdir(path);

			switch(res) {
				case FR_OK:
					return 0;
				case FR_NO_PATH:
				case FR_INVALID_NAME:
				case FR_INVALID_DRIVE:
					errno = ENOENT;
					return -1;
				case FR_DENIED:
				case FR_WRITE_PROTECTED:
					errno = EACCES;
					return -1;
				case FR_EXIST:
					errno = EEXIST;
					return -1;
				case FR_NOT_READY:
				case FR_DISK_ERR:
				case FR_INT_ERR:
				case FR_NOT_ENABLED:
				case FR_NO_FILESYSTEM:
					errno = EIO;
					return -1;
				default:
					/* not supposed to occur according to the libffat documentation */
					Genode::error("f_mkdir() returned an unexpected error code");
					return -1;
			}
		}

		Libc::File_descriptor *open(const char *pathname, int flags) override
		{
			using namespace Ffat;

			if (verbose)
				Genode::log(__func__, ": pathname=", pathname);

			FIL ffat_file;
			BYTE ffat_flags = 0;

			/* convert libc flags to libffat flags */
			if (((flags & O_RDONLY) == O_RDONLY) || ((flags & O_RDWR) == O_RDWR))
				ffat_flags |= FA_READ;

			if (((flags & O_WRONLY) == O_WRONLY)  || ((flags & O_RDWR) == O_RDWR))
				ffat_flags |= FA_WRITE;

			if ((flags & O_CREAT) == O_CREAT) {
				if ((flags & O_EXCL) == O_EXCL)
					ffat_flags |= FA_CREATE_NEW;
				else
					ffat_flags |= FA_OPEN_ALWAYS;
			}

			FRESULT res = f_open(&ffat_file, pathname, ffat_flags);

			switch(res) {
				case FR_OK: {
					Plugin_context *context = new (Genode::env()->heap())
						File_plugin_context(pathname, ffat_file);
					context->status_flags(flags);
					Libc::File_descriptor *fd = Libc::file_descriptor_allocator()->alloc(this, context);
					if ((flags & O_TRUNC) && (ftruncate(fd, 0) == -1))
						return 0;
					return fd;
				}
				case FR_NO_FILE: {
					/*
					 * libc's opendir() calls open() on directories, but
					 * that's not supported by f_open(). So we call
					 * f_opendir() in case the file is a directory.
					 */
					Ffat::DIR ffat_dir;
					FRESULT f_opendir_res = f_opendir(&ffat_dir, pathname);
					if (verbose)
						Genode::log(__func__, ": opendir res=", (int)f_opendir_res);
					switch(f_opendir_res) {
						case FR_OK: {
							Plugin_context *context = new (Genode::env()->heap())
								Directory_plugin_context(pathname, ffat_dir);
							context->status_flags(flags);
							Libc::File_descriptor *f =
								Libc::file_descriptor_allocator()->alloc(this, context);
							if (verbose)
								Genode::log(__func__, ": new fd=", f->libc_fd);
							return f;

						}
						case FR_NO_PATH:
						case FR_INVALID_NAME:
						case FR_INVALID_DRIVE:
							errno = ENOENT;
							return 0;
						case FR_NOT_READY:
						case FR_DISK_ERR:
						case FR_INT_ERR:
						case FR_NOT_ENABLED:
						case FR_NO_FILESYSTEM:
							errno = EIO;
							return 0;
						default:
							/* not supposed to occur according to the libffat documentation */
							Genode::error("f_opendir() returned an unexpected error code");
							return 0;
					}
				}
				case FR_NO_PATH:
				case FR_INVALID_NAME:
				case FR_INVALID_DRIVE:
					errno = ENOENT;
					return 0;
				case FR_EXIST:
					errno = EEXIST;
					return 0;
				case FR_DENIED:
				case FR_WRITE_PROTECTED:
					errno = EACCES;
					return 0;
				case FR_NOT_READY:
				case FR_DISK_ERR:
				case FR_INT_ERR:
				case FR_NOT_ENABLED:
				case FR_NO_FILESYSTEM:
					errno = EIO;
					return 0;
				default:
					/* not supposed to occur according to the libffat documentation */
					Genode::error("f_open() returned an unexpected error code");
					return 0;
			}
		}

		int rename(const char *oldpath, const char *newpath) override
		{
			using namespace Ffat;

			FRESULT res = f_rename(oldpath, newpath);

			switch(res) {
				case FR_OK:
					return 0;
				case FR_NO_FILE:
				case FR_NO_PATH:
				case FR_INVALID_NAME:
				case FR_INVALID_DRIVE:
					errno = ENOENT;
					return 0;
				case FR_EXIST:
					errno = EEXIST;
					return 0;
				case FR_DENIED:
				case FR_WRITE_PROTECTED:
					errno = EACCES;
					return -1;
				case FR_DISK_ERR:
				case FR_INT_ERR:
				case FR_NOT_READY:
				case FR_NOT_ENABLED:
				case FR_NO_FILESYSTEM:
					errno = EIO;
					return -1;
				default:
					/* not supposed to occur according to the libffat documentation */
					Genode::error("f_rename() returned an unexpected error code");
					return -1;
			}
		}

		ssize_t read(Libc::File_descriptor *fd, void *buf, ::size_t count) override
		{
			using namespace Ffat;

			UINT result;
			FRESULT res = f_read(_get_ffat_file(fd), buf, count, &result);

			switch(res) {
				case FR_OK:
					return result;
				case FR_DENIED:
					errno = EACCES;
					return -1;
				case FR_DISK_ERR:
				case FR_INT_ERR:
				case FR_NOT_READY:
				case FR_INVALID_OBJECT:
					errno = EIO;
					return -1;
				default:
					/* not supposed to occur according to the libffat documentation */
					Genode::error("f_read() returned an unexpected error code");
					return -1;
			}
		}

		int stat(const char *path, struct stat *buf) override
		{
			using namespace Ffat;

			FILINFO file_info;
			/* the long file name is not used in this function */
			file_info.lfname = 0;
			file_info.lfsize = 0;

			::memset(buf, 0, sizeof(struct stat));

			/* 'f_stat()' does not work for the '/' directory */
			if (strcmp(path, "/") == 0) {
				buf->st_mode |= S_IFDIR;
				return 0;
			}

			FRESULT res = f_stat(path, &file_info);

			switch(res) {
				case FR_OK:
					break;
				case FR_NO_FILE:
				case FR_NO_PATH:
				case FR_INVALID_NAME:
				case FR_INVALID_DRIVE:
					errno = ENOENT;
					return -1;
				case FR_DISK_ERR:
				case FR_INT_ERR:
				case FR_NOT_READY:
				case FR_NOT_ENABLED:
				case FR_NO_FILESYSTEM:
					errno = EIO;
					return -1;
				default:
					/* not supposed to occur according to the libffat documentation */
					Genode::error("f_stat() returned an unexpected error code");
					return -1;
			}

			/* convert FILINFO to struct stat */
			buf->st_size = file_info.fsize;

			if ((file_info.fattrib & AM_DIR) == AM_DIR) {
				buf->st_mode |= S_IFDIR;
				if (verbose)
					Genode::log(__func__, ": type: directory");
			} else {
				buf->st_mode |= S_IFREG;
				if (verbose)
					Genode::log(__func__, ": type: regular file with a "
					            "size of ", buf->st_size, " bytes");
			}
			/* TODO: handle more attributes */

			struct tm tm;
			tm.tm_year = ((file_info.fdate & 0b1111111000000000) >> 9) + 80;
			tm.tm_mon  =  (file_info.fdate & 0b0000000111100000) >> 5;
			tm.tm_mday =  (file_info.fdate & 0b0000000000011111);
			tm.tm_hour =  (file_info.ftime & 0b1111100000000000) >> 11;
			tm.tm_min  =  (file_info.ftime & 0b0000011111100000) >> 5;
			tm.tm_sec  =  (file_info.ftime & 0b0000000000011111) * 2;

			if (verbose)
				Genode::log("last modified: ",
				            1900 + tm.tm_year, "-", tm.tm_mon, "-", tm.tm_mday, " ",
				            tm.tm_hour, ":", tm.tm_min, ":", tm.tm_sec);

			buf->st_mtime = mktime(&tm);
			if (buf->st_mtime == -1)
				Genode::error("mktime() returned -1, the file modification time reported by stat() will be incorrect");

			return 0;
		}

		int unlink(const char *path) override
		{
			using namespace Ffat;

			FRESULT res = f_unlink(path);

			switch(res) {
				case FR_OK:
					return 0;
				case FR_NO_FILE:
				case FR_NO_PATH:
				case FR_INVALID_NAME:
				case FR_INVALID_DRIVE:
					errno = ENOENT;
					return -1;
				case FR_DENIED:
				case FR_WRITE_PROTECTED:
					errno = EACCES;
					return -1;
				case FR_DISK_ERR:
				case FR_INT_ERR:
				case FR_NOT_READY:
				case FR_NOT_ENABLED:
				case FR_NO_FILESYSTEM:
					errno = EIO;
					return -1;
				default:
					/* not supposed to occur according to the libffat documentation */
					Genode::error("f_unlink() returned an unexpected error code");
					return -1;
			}
		}

		int rmdir(const char *path) override
		{
			return unlink(path);
		}

		ssize_t write(Libc::File_descriptor *fd, const void *buf, ::size_t count) override
		{
			using namespace Ffat;

			UINT result;
			FRESULT res = f_write(_get_ffat_file(fd), buf, count, &result);

			switch(res) {
				case FR_OK:
					return result;
				case FR_DENIED:
					errno = EACCES;
					return -1;
				case FR_DISK_ERR:
				case FR_INT_ERR:
				case FR_NOT_READY:
				case FR_INVALID_OBJECT:
					errno = EIO;
					return -1;
				default:
					/* not supposed to occur according to the libffat documentation */
					Genode::error("f_write() returned an unexpected error code");
					return -1;
			}
		}

		int symlink(const char *, const char *) override
		{
			errno = ENOSYS;
			return -1;
		}
};


} /* unnamed namespace */


void __attribute__((constructor)) init_libc_ffat(void)
{
	static Plugin plugin;
}
