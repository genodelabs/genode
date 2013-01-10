/**
 * \brief  Input driver interface
 * \author Christian Helmuth
 * \date   2006-08-30
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INPUT_H
#define _INPUT_H

#include <input/event.h>

namespace Input_drv {

	/**
	 * Check for pending event
	 */
	bool event_pending();

	/**
	 * Aquire event (this function may block)
	 */
	Input::Event get_event();

	/**
	 * Initialize driver
	 */
	int init();
}

#endif
