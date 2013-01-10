/*
 * \brief  Interface for X input injection
 * \author Norman Feske
 * \date   2009-11-04
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INJECT_INPUT_H_
#define _INJECT_INPUT_H_

/* Genode includes */
#include <input/event.h>

/* X11 includes */
#include <X11/Xlib.h>


/**
 * Initialize X input-injection mechanism
 *
 * \return true on success
 */
bool inject_input_init(Display *dpy);


/**
 * Inject input event to the X session
 */
void inject_input_event(Display *dpy, Input::Event &ev);


#endif /* _INJECT_INPUT_H_ */
