/*
 * \brief libfuse public API
 * \author Josef Soentgen
 * \date 2013-11-07
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _FUSE_OPT_H_
#define _FUSE_OPT_H_

#ifdef __cplusplus
extern "C" {
#endif

struct fuse_opt {
	const char *templ;
	unsigned long off;
	int val;
};

struct fuse_args {
	int argc;
	char **argv;
	int allocated;
};

typedef int (*fuse_opt_proc_t)(void *, const char *, int, struct fuse_args *);
int fuse_opt_add_arg(struct fuse_args *, const char *);
int fuse_opt_insert_arg(struct fuse_args *, int, const char *);
void fuse_opt_free_args(struct fuse_args *);
int fuse_opt_add_opt(char **, const char *);
int fuse_opt_add_opt_escaped(char **, const char *);
int fuse_opt_match(const struct fuse_opt *, const char *);
int fuse_opt_parse(struct fuse_args *, void *, struct fuse_opt *,
    fuse_opt_proc_t);

#define FUSE_ARGS_INIT(ac, av) { ac, av, 0 }

#define FUSE_OPT_KEY(t, k)     { t, -1, k }
#define FUSE_OPT_END           { NULL, 0, 0 }
#define FUSE_OPT_KEY_OPT       -1
#define FUSE_OPT_KEY_NONOPT    -2
#define FUSE_OPT_KEY_KEEP      -3
#define FUSE_OPT_KEY_DISCARD   -4

#ifdef __cplusplus
}
#endif

#endif /* _FUSE_OPT_H_ */
