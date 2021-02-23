/*
 * \brief  VirtualBox runtime (RT)
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2013-08-20
 */

/*
 * Copyright (C) 2013-2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* libc includes */
#include <signal.h>
#include <sys/times.h>
#include <unistd.h>
#include <aio.h>
#include <sched.h>
#include <pthread.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>      /* memset */
#include <sys/mount.h>   /* statfs */
#include <sys/statvfs.h> /* fstatvfs */
#include <fcntl.h>       /* open */

/* local includes */
#include <stub_macros.h>

static bool const debug = true;


extern "C" {

int sched_yield()
{
	static unsigned long counter = 0;

	if (++counter % 100'000 == 0)
		Genode::warning(__func__, " called ", counter, " times");

	return 0;
}

int sched_get_priority_max(int policy) TRACE(0)
int sched_get_priority_min(int policy) TRACE(0)
int pthread_setschedparam(pthread_t thread, int policy,
                          const struct sched_param *param) TRACE(0)
int pthread_getschedparam(pthread_t thread, int *policy,
                          struct sched_param *param) TRACE(0)
int futimes(int fd, const struct timeval tv[2]) TRACE(0)
int lutimes(const char *filename, const struct timeval tv[2]) TRACE(0)
int lchown(const char *pathname, uid_t owner, gid_t group) TRACE(0)
int mlock(const void *addr, size_t len) TRACE(0)
int aio_fsync(int op, struct aiocb *aiocbp) STOP
ssize_t aio_return(struct aiocb *aiocbp) STOP
int aio_error(const struct aiocb *aiocbp) STOP
int aio_cancel(int fd, struct aiocb *aiocbp) STOP
int aio_suspend(const struct aiocb * const aiocb_list[],
                       int nitems, const struct timespec *timeout) STOP
int lio_listio(int mode, struct aiocb *const aiocb_list[],
               int nitems, struct sigevent *sevp) STOP

} /* extern "C" */



/* Helper for VBOXSVC_LOG_DEFAULT hook in global_defs.h */
extern "C" char const * vboxsvc_log_default_string()
{
	char const *vbox_log_string = getenv("VBOX_LOG");

	return vbox_log_string ? vbox_log_string : "";
}


/* used by Shared Folders and RTFsQueryType() in media checking */
extern "C" int statfs(const char *path, struct statfs *buf)
{
	if (!buf) {
		errno = EFAULT;
		return -1;
	}

	int fd = open(path, 0);

	if (fd < 0)
		return fd;

	struct statvfs result;
	int res = fstatvfs(fd, &result);

	close(fd);

	if (res)
		return res;

	memset(buf, 0, sizeof(*buf));

	buf->f_bavail = result.f_bavail;
	buf->f_bfree  = result.f_bfree;
	buf->f_blocks = result.f_blocks;
	buf->f_ffree  = result.f_ffree;
	buf->f_files  = result.f_files;
	buf->f_bsize  = result.f_bsize;

	/* set file-system type to unknown to prevent application of any quirks */
	strcpy(buf->f_fstypename, "unknown");

	bool show_warning = !buf->f_bsize || !buf->f_blocks || !buf->f_bavail;

	if (!buf->f_bsize)
		buf->f_bsize = 4096;
	if (!buf->f_blocks)
		buf->f_blocks = 128 * 1024;
	if (!buf->f_bavail)
		buf->f_bavail = buf->f_blocks;

	if (show_warning)
		Genode::warning("statfs provides bogus values for '", path, "' (probably a shared folder)");

	return res;
}

