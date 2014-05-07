/*
 * \brief  Genode C API balloon functions
 * \author Stefan Kalkowski
 * \date   2013-09-19
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__GENODE__BALLOON_H_
#define _INCLUDE__GENODE__BALLOON_H_

#include <l4/sys/compiler.h>
#include <l4/sys/types.h>

#include <genode/linkage.h>

#ifdef __cplusplus
extern "C" {
#endif

L4_CV l4_cap_idx_t genode_balloon_irq_cap(void);

L4_CV void genode_balloon_free_chunk(unsigned long addr);

L4_CV void genode_balloon_free_done(void);

#ifdef __cplusplus
}
#endif

#endif /* _INCLUDE__GENODE__BALLOON_H_ */
