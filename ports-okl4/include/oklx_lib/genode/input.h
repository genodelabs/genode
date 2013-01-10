/*
 * \brief  Genode C API input functions
 * \author Stefan Kalkowski
 * \date   2010-04-22
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__OKLX_LIB__GENODE__INPUT_H_
#define _INCLUDE__OKLX_LIB__GENODE__INPUT_H_

void genode_input_register_callback(void (*genode_input_event)
                                    (void*,unsigned int, unsigned int, int));

void genode_input_unregister_callback(void);

void genode_input_register_keyb(unsigned int idx, void* dev);

void genode_input_register_mouse(unsigned int idx, void* dev);

void genode_input_unregister_keyb(unsigned int idx);

void genode_input_unregister_mouse(unsigned int idx);

void genode_input_handle_events(void);

#endif //_INCLUDE__OKLX_LIB__GENODE__INPUT_H_
