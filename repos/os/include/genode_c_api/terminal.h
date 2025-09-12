/*
 * \brief  C interface to Genode's terminal session
 * \author Christian Helmuth
 * \date   2025-09-10
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__GENODE_C_API__TERMINAL_H_
#define _INCLUDE__GENODE_C_API__TERMINAL_H_

#include <genode_c_api/base.h>

#ifdef __cplusplus
extern "C" {
#endif

void genode_terminal_init(struct genode_env *,
                          struct genode_allocator *,
                          struct genode_signal_handler *sigh_ptr);

struct genode_terminal; /* definition is private to the implementation */

struct genode_terminal_args
{
	char const *label;
};

struct genode_terminal *genode_terminal_create(struct genode_terminal_args const *);

void genode_terminal_destroy(struct genode_terminal *);

struct genode_terminal_read_ctx; /* definition is private to the implementation */

typedef void (*genode_terminal_read_fn)
	(struct genode_terminal_read_ctx *, struct genode_const_buffer);

void genode_terminal_read(struct genode_terminal *,
                          genode_terminal_read_fn read_fn,
                          struct genode_terminal_read_ctx *);

unsigned long genode_terminal_write(struct genode_terminal *, struct genode_const_buffer);

#ifdef __cplusplus
}
#endif

#endif /* _INCLUDE__GENODE_C_API__TERMINAL_H_ */
