/*
 * \brief  libfuse re-implementation
 * \author Josef Soentgen
 * \date   2013-11-07
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genodes includes */
#include <base/log.h>
#include <util/string.h>

/* libc includes */
#include <stdlib.h>
#include <errno.h>
#include <dirent.h>

#include <fuse.h>
#include <fuse_private.h>

static bool                 _initialized;
static struct fuse         *_fuse;
static struct fuse_context  _ctx;

#if 1
#define TRACE Genode::log("")
#else
#define TRACE
#endif

struct fuse* Fuse::fuse()
{
	if (_fuse == 0) {
		Genode::error("libfuse: struct fuse is zero");
		abort();
	}

	return _fuse;
}

bool Fuse::initialized()
{
	return _initialized;
}

extern "C" {

void fuse_genode(const char *s)
{
	Genode::log(__func__, ": ", s);
}

#define FIX_UP_OPERATION1(f, name) \
	if(f->op.name == 0) \
		f->op.name = dummy_op1;

#define FIX_UP_OPERATION2(f, name) \
	if(f->op.name == 0) \
		f->op.name = dummy_op2;

static int dummy_op1(void)
{
	TRACE;
	return -ENOSYS;
}

static int dummy_op2(void)
{
	TRACE;
	return 0;
}

static int fill_dir(void *dh, const char *name, const struct stat *sbuf, off_t offset)
{
	static uint32_t fileno = 1;

	struct fuse_dirhandle *dir = (struct fuse_dirhandle*)dh;

	if ((dir->offset + sizeof (struct dirent)) > dir->size) {
		Genode::warning("fill_dir buffer full");
		return 1;
	}

	struct dirent *entry = (struct dirent *)(((char*)dir->buf) + dir->offset);
	Genode::memset(entry, 0, sizeof (struct dirent));

	if (sbuf) {
		entry->d_fileno = sbuf->st_ino;
		entry->d_type   = IFTODT(sbuf->st_mode);
	}
	else {
		entry->d_fileno = fileno++;
		entry->d_type   = DT_UNKNOWN;
	}

	/* even in a valid sbuf the inode might by 0 */
	if (entry->d_fileno == 0)
		entry->d_fileno = fileno++;

	size_t namelen = Genode::strlen(name);
	if (namelen > 255)
		namelen = 255;

	Genode::strncpy(entry->d_name, name, namelen + 1);

	entry->d_reclen = sizeof (struct dirent);

	dir->offset += sizeof (struct dirent);

	return 0;
}

/*
 * FUSE API
 */

int fuse_version(void)
{
	return (FUSE_VERSION);
}

int fuse_main(int argc, char *argv[],
                         const struct fuse_operations *fsop, void *data)
{
	TRACE;
	return -1;
}

struct fuse* fuse_new(struct fuse_chan *chan, struct fuse_args *args,
                                 const struct fuse_operations *fsop, size_t size,
                                 void *userdata)
{
	TRACE;

	_fuse = reinterpret_cast<struct fuse*>(malloc(sizeof (struct fuse)));
	if (_fuse == 0) {
		Genode::error("could not create struct fuse");
		return 0;
	}

	_fuse->fc = chan;
	_fuse->args = args;

	Genode::memcpy(&_fuse->op, fsop, Genode::min(size, sizeof (_fuse->op)));

	/**
	 * Defining a dummy function for each fuse operation is cumbersome.
	 * So let us faithfully ignore the compiler.
	 */
#pragma GCC diagnostic ignored "-fpermissive"

	FIX_UP_OPERATION1(_fuse, readlink);
	FIX_UP_OPERATION1(_fuse, mknod);
	FIX_UP_OPERATION1(_fuse, unlink);
	FIX_UP_OPERATION1(_fuse, rmdir);
	FIX_UP_OPERATION1(_fuse, symlink);
	FIX_UP_OPERATION1(_fuse, rename);
	FIX_UP_OPERATION1(_fuse, link);
	FIX_UP_OPERATION1(_fuse, chmod);
	FIX_UP_OPERATION1(_fuse, chown);
	FIX_UP_OPERATION1(_fuse, truncate);
	FIX_UP_OPERATION1(_fuse, utime);
	FIX_UP_OPERATION2(_fuse, open);
	FIX_UP_OPERATION1(_fuse, read);
	FIX_UP_OPERATION1(_fuse, write);
	FIX_UP_OPERATION1(_fuse, statfs);
	FIX_UP_OPERATION1(_fuse, flush);
	FIX_UP_OPERATION1(_fuse, release);
	FIX_UP_OPERATION1(_fuse, fsync);
	FIX_UP_OPERATION1(_fuse, fsyncdir);
	FIX_UP_OPERATION1(_fuse, access);
	FIX_UP_OPERATION1(_fuse, create);
	FIX_UP_OPERATION1(_fuse, utimens);
	FIX_UP_OPERATION1(_fuse, read);
	FIX_UP_OPERATION1(_fuse, read);

	if (_fuse->op.opendir == 0)
		_fuse->op.opendir = _fuse->op.open;
	if (_fuse->op.releasedir == 0)
		_fuse->op.releasedir = _fuse->op.release;
	if (_fuse->op.fgetattr == 0)
		_fuse->op.fgetattr = _fuse->op.getattr;
	if (_fuse->op.ftruncate == 0)
		_fuse->op.ftruncate = _fuse->op.truncate;

	_fuse->filler = fill_dir;

	_fuse->userdata = userdata;

	_ctx.fuse         = _fuse;
	_ctx.uid          = 0;
	_ctx.gid          = 0;
	_ctx.pid          = 0;
	_ctx.private_data = userdata;
	_ctx.umask        = 022;

	_initialized      = true;

	return _fuse;
}

int fuse_parse_cmdline(struct fuse_args *args, char **mp, int *mt, int *fg)
{
	TRACE;
	return -1;
}

struct fuse_chan* fuse_mount(const char *dir, struct fuse_args *args)
{
	TRACE;
	return 0;
}

void fuse_remove_signal_handlers(struct fuse_session *se)
{
	TRACE;
}

int fuse_set_signal_handlers(struct fuse_session *se)
{
	TRACE;
	return -1;
}

struct fuse_session* fuse_get_session(struct fuse *f)
{
	TRACE;
	return 0;
}

struct fuse_context* fuse_get_context(void)
{
	return &_ctx;
}

int fuse_is_lib_option(const char *opt)
{
	TRACE;
	return -1;
}

int fuse_loop(struct fuse *fuse)
{
	TRACE;
	return -1;
}

int fuse_loop_mt(struct fuse *fuse)
{
	TRACE;
	return -1;
}

int fuse_chan_fd(struct fuse_chan *ch)
{
	TRACE;
	return -1;
}

void fuse_unmount(const char *dir, struct fuse_chan *ch)
{
	TRACE;
}

int fuse_daemonize(int foreground)
{
	TRACE;
	return -1;
}

void fuse_destroy(struct fuse *f)
{
	TRACE;

	free(f);
}

void fuse_teardown(struct fuse *f, char *mp)
{
	TRACE;
}

int fuse_opt_add_arg(struct fuse_args *, const char *)
{
	TRACE;
	return -1;
}

void fuse_opt_free_args(struct fuse_args *)
{
	TRACE;
}

} /* extern "C" */
