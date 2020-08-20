/*
 * \brief  POSIX wrappers for important base lib features
 * \author Christian Prochaska
 * \date   2020-07-06
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__LIBC__GENODE_H_
#define _INCLUDE__LIBC__GENODE_H_

/* libc includes */
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void genode_cache_coherent(void *addr, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* _INCLUDE__LIBC__GENODE_H_ */
