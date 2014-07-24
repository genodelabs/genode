/*
 * \brief  Input service and event handler
 * \author Christian Helmuth
 * \author Dirk Vogt <dvogt@os.inf.tu-dresden.de>
 * \author Sebastian Sumpf <sebastian.sumpf@genode-labs.com>
 * \date   2009-04-20
 *
 * The original implementation was in the L4Env from the TUD:OS group
 * (l4/pkg/input/lib/src/l4evdev.c). This file was released under the terms of
 * the GNU General Public License version 2.
 */

/*
 * Copyright (C) 2014 Ksys Labs LLC
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Linux */
#include <linux/input.h>

/* Local */
#include <lx_emul.h>

/* Callback function to Genode subsystem */
static genode_input_event_cb handler;;

void genode_evdev_event(struct input_handle *handle, unsigned int type,
                        unsigned int code, int value)
{
#if DEBUG_EVDEV
	static unsigned long count = 0;
#endif

	static int last_ax = -1; // store the last absolute x value 
	  
	/* filter sound events */
	if (test_bit(EV_SND, handle->dev->evbit)) return;

	/* filter input_repeat_key() */
	if ((type == EV_KEY) && (value == 2)) return;

	/* filter EV_SYN */
	if (type == EV_SYN) return;

	/* generate arguments and call back */
	enum input_event_type arg_type;
	unsigned arg_keycode = KEY_UNKNOWN;
	int arg_ax = 0, arg_ay = 0, arg_rx = 0, arg_ry = 0;

	switch (type) {

	case EV_KEY:
		arg_keycode = code;
		switch (value) {

		case 0:
			arg_type = EVENT_TYPE_RELEASE;
			if(code == 0x14a)           // XXX map BTN_TOUCH events to BTN_LEFT
			  arg_keycode = 0x110;
			break;

		case 1:
			arg_type = EVENT_TYPE_PRESS;
			if(code == 0x14a)           // XXX
			  arg_keycode = 0x110;
			break;

		default:
			printk("Unknown key event value %d - not handled\n", value);
			return;
		}
		break;

	case EV_ABS:
		switch (code) {

		case ABS_X:             // Don't create an input event yet. Store the value and wait for the subsequent Y event.
		case ABS_MT_POSITION_X: // XXX treat every MT Position event as a normal Mouse event
			last_ax = value;
			return;

		case ABS_Y:             // Create a unified input event with absolute positions on x and y axis.
		case ABS_MT_POSITION_Y: // XXX treat every MT Position event as a normal Mouse event
			arg_type = EVENT_TYPE_MOTION;
			arg_ay = value;
			arg_ax = last_ax;
			last_ax = -1;
			if (arg_ax == -1) {
				printk("Ignore absolute Y event without a preceeding X event\n");
				return;
			}
			break;

		case ABS_WHEEL:
			/*
			 * XXX I do not know, how to handle this correctly. At least, this
			 * scheme works on Qemu.
			 */
			arg_type = EVENT_TYPE_WHEEL;
			arg_ry = value;
			break;

		default:
			printk("Unknown absolute event code %d - not handled\n", code);
			return;
		}
		break;

	case EV_REL:
		switch (code) {

		case REL_X:
			arg_type = EVENT_TYPE_MOTION;
			arg_rx = value;
			break;

		case REL_Y:
			arg_type = EVENT_TYPE_MOTION;
			arg_ry = value;
			break;

		case REL_HWHEEL:
			arg_type = EVENT_TYPE_WHEEL;
			arg_rx = value;
			break;

		case REL_WHEEL:
			arg_type = EVENT_TYPE_WHEEL;
			arg_ry = value;
			break;

		default:
			printk("Unknown relative event code %d - not handled\n", code);
			return;
		}
		break;

	default:
		printk("Unknown event type %d - not handled\n", type);
		return;
	}

	if (handler)
		handler(arg_type, arg_keycode, arg_ax, arg_ay, arg_rx, arg_ry);
		printk("EVENT: t: %x c: %x ax: %d ay %d rx: %d ry %d\n",
		       arg_type, arg_keycode, arg_ax, arg_ay, arg_rx, arg_ry);
#if DEBUG_EVDEV
	printk("event[%ld]. dev: %s, type: %d, code: %d, value: %d\n",
	       count++, handle->dev->name, type, code, value);
#endif
}

void genode_input_register(genode_input_event_cb h) { handler = h; }

