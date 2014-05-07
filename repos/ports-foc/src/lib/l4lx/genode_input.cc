/*
 * \brief  Genode C API input functions for the Linux support libary
 * \author Stefan Kalkowski
 * \date   2010-04-21
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/env.h>
#include <input_session/connection.h>
#include <input/event.h>
#include <input/keycodes.h>

#include <linux.h>
#include <env.h>

enum Event_types   {EV_SYN=0x00, EV_KEY=0x01, EV_REL=0x02, EV_ABS=0x03 };
enum Relative_axes {REL_X=0x00, REL_Y=0x01, REL_WHEEL=0x08 };
enum Absolute_axes {ABS_X=0x00, ABS_Y=0x01, ABS_WHEEL=0x08 };


static Input::Connection *input() {
	static Input::Connection _inp;
	return &_inp;
}


static Input::Event *buffer() {
	static Input::Event *_ev_buf = 0;
	if (!_ev_buf) {
		Linux::Irq_guard guard;

		_ev_buf = (Input::Event*) L4lx::Env::env()->rm()->attach(input()->dataspace(),
		                                                         "input buffer");
	}
	return _ev_buf;
}


extern "C" {

#include <genode/input.h>

	FASTCALL static void (*genode_input_event) (void*,unsigned int, unsigned int, int) = 0;
	static void *keyboard = 0;
	static void *mouse    = 0;

	void genode_input_register_callback(FASTCALL void (*func)
	                                    (void*,unsigned int, unsigned int, int))
	{
		genode_input_event = func;
	}


	void genode_input_unregister_callback(void)
	{
		genode_input_event = 0;
	}


	void genode_input_register_keyb(unsigned int idx, void* dev)
	{
		keyboard = dev;
	}


	void genode_input_unregister_keyb(unsigned int idx)
	{
		keyboard = 0;
	}


	void genode_input_register_mouse(unsigned int idx, void* dev)
	{
		mouse = dev;
	}


	void genode_input_unregister_mouse(unsigned int idx)
	{
		mouse = 0;
	}


	static void handle_event(void *mouse, void *keyb, Input::Event *ev)
	{
		using namespace Input;

		switch(ev->type()) {
		case Event::MOTION:
			{
				if(ev->rx())
					genode_input_event(mouse, EV_REL, REL_X, ev->rx());
				if(ev->ry())
					genode_input_event(mouse, EV_REL, REL_Y, ev->ry());
				if(ev->ax())
					genode_input_event(mouse, EV_ABS, ABS_X, ev->ax());
				if(ev->ay())
					genode_input_event(mouse, EV_ABS, ABS_Y, ev->ay());
				return;
			}
		case Event::PRESS:
			{
				if (ev->code() < BTN_MISC)
					genode_input_event(keyb, EV_KEY, ev->code(), 1);
				else
					genode_input_event(mouse, EV_KEY, ev->code(), 1);
				return;
			}
		case Event::RELEASE:
			{
				if (ev->code() < BTN_MISC)
					genode_input_event(keyb, EV_KEY, ev->code(), 0);
				else
					genode_input_event(mouse, EV_KEY, ev->code(), 0);
				return;
			}
		case Event::WHEEL:
			{
				if(ev->rx())
					genode_input_event(mouse, EV_REL, REL_WHEEL, ev->rx());
				else
					genode_input_event(mouse, EV_ABS, ABS_WHEEL, ev->ax());
				return;
			}
		case Event::INVALID:
		default:
			;
		}
	}


	void genode_input_handle_events(void)
	{
		if (!genode_input_event)
			return;

		unsigned long flags;
		l4x_irq_save(&flags);

		if ( mouse && keyboard && input()) {
			int num = input()->flush();
			l4x_irq_restore(flags);
			for (int i = 0; i < num; i++) {
				Input::Event ev = buffer()[i];
				handle_event(mouse, keyboard, &ev);
			}
		} else
			l4x_irq_restore(flags);
	}

} //extern "C"
