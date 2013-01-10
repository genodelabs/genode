/*
 * \brief  Genode C API input functions needed by Linux
 * \author Stefan Kalkowski
 * \date   2009-06-08
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__GENODE__INPUT_H_
#define _INCLUDE__GENODE__INPUT_H_


#include <l4/sys/compiler.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef i386
#define FASTCALL __attribute__((regparm(3)))
#else
#define FASTCALL
#endif

L4_CV void genode_input_register_callback(FASTCALL void (*genode_input_event)
                                          (void*,unsigned int, unsigned int, int));

L4_CV void genode_input_unregister_callback(void);

L4_CV void genode_input_register_keyb(unsigned int idx, void* dev);

L4_CV void genode_input_register_mouse(unsigned int idx, void* dev);

L4_CV void genode_input_unregister_keyb(unsigned int idx);

L4_CV void genode_input_unregister_mouse(unsigned int idx);

L4_CV void genode_input_handle_events(void);

#ifdef __cplusplus
}
#endif


#endif //_INCLUDE__GENODE__INPUT_H_
