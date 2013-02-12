/*
 * \brief  Keyboard manager
 * \author Markus Partheymueller
 * \date   2012-07-31
 */

/*
 * Copyright (C) 2012 Intel Corporation    
 * 
 * This file is part of the Genode OS framework and being contributed under
 * the terms and conditions of the GNU General Public License version 2.   
 */

#ifndef _VANCOUVER_KEYBOARD_H
#define _VANCOUVER_KEYBOARD_H

/* NOVA userland includes */
#include <nul/motherboard.h>

/* Includes for I/O */
#include <base/env.h>
#include <input/event.h>
#include <input/keycodes.h>

class Vancouver_keyboard {
	private:
		Motherboard &_mb;
		unsigned     _flags;

	public:
		/*
		 * Constructor
		 */
		Vancouver_keyboard(Motherboard &mb);

		void handle_keycode_press(unsigned keycode);
		void handle_keycode_release(unsigned keycode);
};

#endif
