/*
 * \brief libfuse re-implementation public API
 * \author Josef Soentgen
 * \date 2013-11-07
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__FUSE_H_
#define _INCLUDE__FUSE_H_

/* libc includes */
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <fcntl.h>
#include <utime.h>


#ifdef __cplusplus
extern "C" {
#endif

#ifndef FUSE_USE_VERSION
#define FUSE_USE_VERSION 26
#endif

#if FUSE_USE_VERSION >= 26
#define FUSE_VERSION 26
#else
#error "No support for FUSE_VERSION < 26"
#endif

void fuse_genode(const char*);

#include "fuse_opt.h"

struct fuse_chan;
struct fuse_args;
struct fuse_session;

struct fuse_file_info {
	int32_t  flags;            /* open(2) flags */
	uint32_t fh_old;           /* old file handle */
	int32_t  writepage;
	uint32_t direct_io:1;
	uint32_t keep_cache:1;
	uint32_t flush:1;
	uint32_t nonseekable:1;
	uint32_t __padd:27;
	uint32_t flock_release : 1;
	uint64_t fh;                /* file handle */
	uint64_t lock_owner;
};

struct fuse_conn_info {
	uint32_t proto_major;
	uint32_t proto_minor;
	uint32_t async_read;
	uint32_t max_write;
	uint32_t max_readahead;
	uint32_t capable;
	uint32_t want;
	uint32_t max_background;
	uint32_t congestion_threshold;
	uint32_t reserved[23];
};

struct fuse_context {
	struct fuse *fuse;
	uid_t        uid;
	gid_t        gid;
	pid_t        pid;
	void        *private_data;
	mode_t       umask;
};

typedef ino_t fuse_ino_t;
typedef int (*fuse_fill_dir_t)(void *, const char *, const struct stat *, off_t);



typedef int (*fuse_dirfil_t)(void *, const char *, int, ino_t);

/* only operations available in 2.6 */
struct fuse_operations {
	int	(*getattr)(const char *, struct stat *);		
	int	(*readlink)(const char *, char *, size_t);
	int	(*getdir)(const char *, void *, fuse_dirfil_t);
	int	(*mknod)(const char *, mode_t, dev_t);
	int	(*mkdir)(const char *, mode_t);		
	int	(*unlink)(const char *);		
	int	(*rmdir)(const char *);
	int	(*symlink)(const char *, const char *);		
	int	(*rename)(const char *, const char *);		
	int	(*link)(const char *, const char *);
	int	(*chmod)(const char *, mode_t);				
	int	(*chown)(const char *, uid_t, gid_t);			
	int	(*truncate)(const char *, off_t);
	int	(*utime)(const char *, struct utimbuf *);
	int	(*open)(const char *, struct fuse_file_info *);		
	int	(*read)(const char *, char *, size_t, off_t,
		struct fuse_file_info *);		
	int	(*write)(const char *, const char *, size_t, off_t,
		struct fuse_file_info *);		
	int	(*statfs)(const char *, struct statvfs *);
	int	(*flush)(const char *, struct fuse_file_info *);
	int	(*release)(const char *, struct fuse_file_info *);		
	int	(*fsync)(const char *, int, struct fuse_file_info *);
	int	(*setxattr)(const char *, const char *, const char *, size_t,
		int);
	int	(*getxattr)(const char *, const char *, char *, size_t);
	int	(*listxattr)(const char *, char *, size_t);
	int	(*removexattr)(const char *, const char *);
	int	(*opendir)(const char *, struct fuse_file_info *);
	int	(*readdir)(const char *, void *, fuse_fill_dir_t, off_t,
		struct fuse_file_info *);
	int	(*releasedir)(const char *, struct fuse_file_info *);
	int	(*fsyncdir)(const char *, int, struct fuse_file_info *);
	void	*(*init)(struct fuse_conn_info *);
	void	(*destroy)(void *);
	int	(*access)(const char *, int);		
	int	(*create)(const char *, mode_t, struct fuse_file_info *);
	int	(*ftruncate)(const char *, off_t, struct fuse_file_info *);
	int	(*fgetattr)(const char *, struct stat *, struct fuse_file_info *);
	int	(*lock)(const char *, struct fuse_file_info *, int, struct flock *);
	int	(*utimens)(const char *, const struct timespec *);
	int	(*bmap)(const char *, size_t , uint64_t *);
};

/* API */
int                  fuse_version(void);
struct fuse         *fuse_new(struct fuse_chan *, struct fuse_args *,
                              const struct fuse_operations *, size_t, void *);
void                 fuse_destroy(struct fuse *);
void                 fuse_exit(struct fuse *f);
struct fuse_session *fuse_get_session(struct fuse *);
struct fuse_context *fuse_get_context(void);
int                  fuse_loop(struct fuse *);
int                  fuse_loop_mt(struct fuse *);
int                  fuse_main(int, char **, const struct fuse_operations *, void *);
struct fuse_chan    *fuse_mount(const char *, struct fuse_args *);
int                  fuse_parse_cmdline(struct fuse_args *, char **, int *, int *);
void                 fuse_remove_signal_handlers(struct fuse_session *);
int                  fuse_set_signal_handlers(struct fuse_session *);
int                  fuse_chan_fd(struct fuse_chan *);
int                  fuse_daemonize(int);
int                  fuse_is_lib_option(const char *);
void                 fuse_teardown(struct fuse *, char *);
void                 fuse_unmount(const char *, struct fuse_chan *);

#ifdef __cplusplus
}
#endif

#endif /* _INCLUDE__FUSE_H_ */
