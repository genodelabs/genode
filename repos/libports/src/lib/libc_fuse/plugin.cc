/*
 * \brief  Libc libfuse plugin
 * \author Josef Soentgen
 * \date   2013-11-07
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/allocator_avl.h>
#include <base/env.h>
#include <base/log.h>
#include <util/string.h>
#include <os/path.h>

/* libc plugin includes */
#include <libc-plugin/plugin.h>
#include <libc-plugin/fd_alloc.h>

/* fuse */
#include <fuse_private.h>

/* libc includes */
#include <sys/statvfs.h>
#include <sys/dirent.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

using namespace Genode;

void *operator new (size_t, void *ptr) { return ptr; }

/* a little helper to prevent code duplication */
static inline int check_result(int res)
{
	if (res < 0) {
		/**
		 * FUSE file systems always return -errno as result
		 * if something went wrong.
		 */
		errno = -res;
		return -1;
	}

	return 0;
}


/****************************
 ** override libc defaults **
 ****************************/

extern "C" int chmod(const char *path, mode_t mode)
{
	return check_result(Fuse::fuse()->op.chmod(path, mode));
}


extern "C" int chown(const char *path, uid_t uid, gid_t gid)
{
	return check_result(Fuse::fuse()->op.chown(path, uid, gid));
}


extern "C" int link(const char *oldpath, const char *newpath)
{
	return check_result(Fuse::fuse()->op.link(oldpath, newpath));
}


/************
 ** Plugin **
 ************/

namespace {

	struct Plugin_context : Libc::Plugin_context
	{
		String<4096>          path;
		int                   flags;
		int                   fd_flags;
		struct fuse_file_info file_info;

		::off_t               offset;

		Plugin_context(const char *p, int f)
		:
			path(p), flags(f), offset(0)
		{
			Genode::memset(&file_info, 0, sizeof (struct fuse_file_info));
		}

		~Plugin_context() { }
	};

	static inline Plugin_context *context(Libc::File_descriptor *fd)
	{
		return static_cast<Plugin_context *>(fd->context);
	}

	class Plugin : public Libc::Plugin
	{
		private:

			enum { PLUGIN_PRIORITY = 1 };

		public:

			/**
			 * Constructor
			 */
			Plugin() : Libc::Plugin(PLUGIN_PRIORITY)
			{
				if (!Fuse::init_fs()) {
					error("FUSE fs initialization failed");
					return;
				}
			}

			~Plugin()
			{
				if (Fuse::initialized())
					Fuse::deinit_fs();
			}

			bool supports_mkdir(const char *path, mode_t mode)
			{
				if (Fuse::initialized() == 0)
					return false;

				return true;
			}

			bool supports_open(const char *pathname, int flags)
			{
				if (Genode::strcmp(pathname, "/dev/blkdev") == 0)
					return false;

				if (Fuse::initialized() == 0)
					return false;

				return true;
			}

			bool supports_readlink(const char *path, char *buf, ::size_t bufsiz)
			{
				if (Fuse::initialized() == 0)
					return false;

				return true;
			}

			bool supports_rmdir(const char *path)
			{
				if (Fuse::fuse() == 0)
					return false;
				return true;
			}

			bool supports_stat(const char *path)
			{
				if (Fuse::initialized() == 0)
					return false;

				return true;
			}

			bool supports_symlink(const char *oldpath, const char *newpath)
			{
				if (Fuse::fuse() == 0)
					return false;
				return true;
			}

			bool supports_unlink(const char *path)
			{
				if (Fuse::fuse() == 0)
					return false;
				return true;
			}


			int close(Libc::File_descriptor *fd)
			{
				Plugin_context *ctx = context(fd);

				Fuse::fuse()->op.release(ctx->path.string(), &ctx->file_info);

				destroy(env()->heap(), ctx);
				Libc::file_descriptor_allocator()->free(fd);

				return 0;
			}

			int fcntl(Libc::File_descriptor *fd, int cmd, long arg)
			{
				Plugin_context *ctx = context(fd);

				switch (cmd) {
				case F_GETFD:
					return ctx->fd_flags;
				case F_GETFL:
					return ctx->flags;
				case F_SETFD:
					ctx->fd_flags = arg;
					return 0;
				default:
					warning(__func__, ": cmd ", cmd, " not supported");
					return -1;
				}

				return -1; /* never reached */
			}

			int fstat(Libc::File_descriptor *fd, struct stat *buf)
			{
				Plugin_context *ctx = context(fd);

				Genode::memset(buf, 0, sizeof (struct stat));

				int res = Fuse::fuse()->op.getattr(ctx->path.string(), buf);
				if (res != 0) {
					errno = -res;
					return -1;
				}

				return 0;
			}

			int fstatfs(Libc::File_descriptor *fd, struct statfs *buf)
			{
				Plugin_context *ctx = context(fd);

				struct statvfs vfs;

				int res = Fuse::fuse()->op.statfs(ctx->path.string(), &vfs);
				if (res != 0) {
					errno = -res;
					return -1;
				}

				Genode::memset(buf, 0, sizeof (struct statfs));

				buf->f_bsize     = vfs.f_bsize;
				//buf->f_frsize    = vfs.f_frsize;
				buf->f_blocks    = vfs.f_blocks;
				buf->f_bavail    = vfs.f_bavail;
				buf->f_bfree     = vfs.f_bfree;
				buf->f_namemax = vfs.f_namemax;
				buf->f_files   = vfs.f_files;
				//buf->f_favail  = vfs.f_favail;
				buf->f_ffree   = vfs.f_ffree;

				return 0;
			}

			int ftruncate(Libc::File_descriptor *fd, ::off_t length)
			{
				Plugin_context *ctx = context(fd);

				int res = Fuse::fuse()->op.ftruncate(ctx->path.string(), length,
				                                     &ctx->file_info);
				if (res != 0) {
					errno = -res;
					return -1;
				}

				return 0;
			}

			::ssize_t getdirentries(Libc::File_descriptor *fd, char *buf, ::size_t nbytes,
			                        ::off_t *basep)
			{
				Plugin_context *ctx = context(fd);

				if (nbytes < sizeof (struct dirent)) {
					error(__func__, ": buf too small");
					errno = ENOMEM;
					return -1;
				}

				struct dirent *de = (struct dirent *)buf;
				::memset(de, 0, sizeof (struct dirent));

				struct fuse_dirhandle dh = {
					.filler = Fuse::fuse()->filler,
					.buf    = buf,
					.size   = nbytes,
					.offset = 0,
				};

				int res = Fuse::fuse()->op.readdir(ctx->path.string(), &dh,
				                                   Fuse::fuse()->filler, 0,
				                                   &ctx->file_info);
				if (res != 0) {
					errno = -res;
					return -1;
				}

				/**
				 * We have to stat(2) each entry because there are FUSE file
				 * systems which do not provide a valid struct stat entry in
				 * its readdir() implementation because only d_ino and d_name
				 * are specified by POSIX.
				 */
				::off_t offset = 0;
				while (offset < dh.offset) {
					struct dirent *entry = (struct dirent*)(buf + offset);

					/* try to query the type of the file if the type is unknown  */
					if (entry->d_type == DT_UNKNOWN) {
						Genode::Path<4096> path(entry->d_name, ctx->path.string());
						struct stat sbuf;
						res = Fuse::fuse()->op.getattr(path.base(), &sbuf);
						if (res == 0) {
							entry->d_type   = IFTODT(sbuf.st_mode);
							entry->d_fileno = sbuf.st_ino ? sbuf.st_ino : 1;
						}
					}

					offset += sizeof (struct dirent);
				}

				/**
				 * To prevent the libc from being stuck in an endless loop we
				 * append an empty entry. This is a rather hacky solution but
				 * for now it suffice.
				 */
				dh.offset += sizeof (struct dirent);
				((struct dirent*)(buf + dh.offset))->d_reclen = 0;

				return dh.offset;
			}

			::off_t lseek(Libc::File_descriptor *fd, ::off_t offset, int whence)
			{
				Plugin_context *ctx = context(fd);

				switch (whence) {
				case SEEK_SET:
					ctx->offset = offset;
					return ctx->offset;

				case SEEK_CUR:
					ctx->offset += offset;
					return ctx->offset;

				case SEEK_END:
					if (offset != 0) {
						errno = EINVAL;
						return -1;
					}
					ctx->offset = ~0L;

					return (Fuse::fuse()->block_size * Fuse::fuse()->block_count);

				default:
					errno = EINVAL;
					return -1;
				}
			}

			int mkdir(const char *pathname, mode_t mode)
			{
				int res = Fuse::fuse()->op.mkdir(pathname, mode);

				return check_result(res);
			}

			Libc::File_descriptor *open(const char *pathname, int flags)
			{
				/* XXX evaluate flags */

				Plugin_context *context = new (Genode::env()->heap())
					Plugin_context(pathname, flags);

				int res;
				int tries = 0;
				do {
					/* first try to open pathname */
					res = Fuse::fuse()->op.open(pathname, &context->file_info);
					if (res == 0) {
						break;
					}

					/* try to create pathname if open failed and O_CREAT flag was specified */
					if (flags & O_CREAT && !tries) {
						mode_t mode = S_IFREG | 0644;
						int res = Fuse::fuse()->op.mknod(pathname, mode, 0);
						switch (res) {
							case 0:
								break;
							default:
								error(__func__, ": could not create '", pathname, "'");
								destroy(env()->heap(), context);
								return 0;
						}

						tries++;
						continue;
					}

					if (res < 0) {
						errno = -res;
						destroy(env()->heap(), context);
						return 0;
					}
				}
				while (true);

				if (flags & O_TRUNC) {
					res = Fuse::fuse()->op.ftruncate(pathname, 0,
					                                     &context->file_info);
					if (res != 0) {
						errno = -res;
						Fuse::fuse()->op.release(context->path.string(),
						                         &context->file_info);
						destroy(env()->heap(), context);
						return 0;
					}
				}

				context->file_info.flags = flags;

				return Libc::file_descriptor_allocator()->alloc(this, context);
			}

			ssize_t read(Libc::File_descriptor *fd, void *buf, ::size_t count)
			{
				Plugin_context *ctx = context(fd);

				int res = Fuse::fuse()->op.read(ctx->path.string(),
				                                reinterpret_cast<char*>(buf),
				                                count, ctx->offset, &ctx->file_info);

				if (check_result(res))
					return -1;

				ctx->offset += res;

				return res;
			}

			ssize_t readlink(const char *path, char *buf, ::size_t bufsiz)
			{
				int res = Fuse::fuse()->op.readlink(path, buf, bufsiz);
				if (res < 0) {
					errno = -res;
					return -1;
				}

				/**
				 * We have to trust each FUSE file system to append a
				 * null byte to buf, which is required according to
				 * FUSEs documentation.
				 */
				return Genode::strlen(buf);
			}

			int rename(const char *oldpath, const char *newpath)
			{
				int res = Fuse::fuse()->op.rename(oldpath, newpath);

				return check_result(res);
			}

			int rmdir(const char *path)
			{
				int res = Fuse::fuse()->op.rmdir(path);

				return check_result(res);
			}

			int stat(const char *path, struct stat *buf)
			{
				Genode::memset(buf, 0, sizeof (buf));

				int res = Fuse::fuse()->op.getattr(path, buf);

				return check_result(res);
			}

			int symlink(const char *oldpath, const char *newpath)
			{
				int res = Fuse::fuse()->op.symlink(oldpath, newpath);

				return check_result(res);
			}

			int unlink(const char *path)
			{
				int res = Fuse::fuse()->op.unlink(path);

				return check_result(res);
			}

			ssize_t write(Libc::File_descriptor *fd, const void *buf, ::size_t count)
			{
				Plugin_context *ctx = context(fd);

				int res = Fuse::fuse()->op.write(ctx->path.string(),
				                                 reinterpret_cast<const char*>(buf),
				                                 count, ctx->offset, &ctx->file_info);

				if (check_result(res))
					return -1;

				ctx->offset += res;

				return res;
			}

	};

} /* unnamed namespace */


void __attribute__((constructor)) init_libc_fuse(void)
{
	/*
	 * During the initialization of the plugin, we already require the VFS.
	 * Hence, we need to make sure to initialize the VFS before doing our
	 * own initialization.
	 */
	extern void init_libc_vfs(void);
	init_libc_vfs();

	static Plugin plugin;
}
