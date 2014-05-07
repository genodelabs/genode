/*
 * \brief  L4Re functions needed by L4Linux
 * \author Stefan Kalkowski
 * \date   2011-03-17
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _L4__RE__C__DEBUG_H_
#define _L4__RE__C__DEBUG_H_

#include <l4/sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

L4_CV void l4re_debug_obj_debug(l4_cap_idx_t srv, unsigned long function);

#ifdef __cplusplus
}
#endif

#endif /* _L4__RE__C__DEBUG_H_ */
