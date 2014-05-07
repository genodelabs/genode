/*
 * \brief  Genode C API serial/console driver related functions needed by L4Linux
 * \author Stefan Kalkowski
 * \date   2011-09-16
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__GENODE__TERMINAL_H_
#define _INCLUDE__GENODE__TERMINAL_H_


#include <l4/sys/compiler.h>
#include <l4/sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

L4_CV signed char  genode_terminal_readchar(unsigned idx, char *buf, unsigned long sz);
L4_CV void         genode_terminal_writechar(unsigned idx, const char *buf, unsigned long sz);
L4_CV l4_cap_idx_t genode_terminal_irq(unsigned idx);
L4_CV void         genode_terminal_stop(unsigned idx);
L4_CV unsigned     genode_terminal_count(void);

#ifdef __cplusplus
}
#endif

#endif //_INCLUDE__GENODE__TERMINAL_H_
