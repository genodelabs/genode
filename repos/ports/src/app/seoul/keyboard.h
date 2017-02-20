/*
 * \brief  Keyboard manager
 * \author Markus Partheymueller
 * \date   2012-07-31
 */

/*
 * Copyright (C) 2012 Intel Corporation
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 *
 * The code is partially based on the Vancouver VMM, which is distributed
 * under the terms of the GNU General Public License version 2.
 *
 * Modifications by Intel Corporation are contributed under the terms and
 * conditions of the GNU General Public License version 2.
 */

#ifndef _KEYBOARD_H_
#define _KEYBOARD_H_


/* includes for I/O */
#include <base/env.h>
#include <input/event.h>
#include <input/keycodes.h>

/* local includes */
#include "synced_motherboard.h"

namespace Seoul {
	class Keyboard;
}

class Seoul::Keyboard
{
	private:

		Synced_motherboard &_motherboard;
		unsigned            _flags;

		bool _map_keycode(unsigned &, bool);

	public:

		/**
		 * Constructor
		 */
		Keyboard(Synced_motherboard &);

		void handle_keycode_press(unsigned keycode);
		void handle_keycode_release(unsigned keycode);
};

#endif /* _KEYBOARD_H_ */
