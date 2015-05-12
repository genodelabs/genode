/*
 * \brief  Input service and event handler
 * \author Christian Helmuth
 * \author Dirk Vogt        <dvogt@os.inf.tu-dresden.de>
 * \author Sebastian Sumpf  <sebastian.sumpf@genode-labs.com>
 * \author Christian Menard <christian.menard@ksyslabs.org>
 * \author Alexander Boettcher <alexander.boettcher@genode-labs.com>
 * \date   2009-04-20
 *
 * The original implementation was in the L4Env from the TUD:OS group
 * (l4/pkg/input/lib/src/l4evdev.c). This file was released under the terms of
 * the GNU General Public License version 2.
 */

/*
 * Copyright (C) 2009-2015 Genode Labs GmbH
 * Copyright (C) 2014      Ksys Labs LLC
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

static unsigned long screen_x  = 0;
static unsigned long screen_y  = 0;
static bool disable_multitouch = true;
static bool disable_absolute   = false;

static struct slot
{
	int id;    /* current tracking id  */
	int x;     /* last reported x axis */
	int y;     /* last reported y axis */
	int event; /* last reported ABS_MT_ event */
} slots[16];


static bool transform(struct input_handle *handle, int x, int y,
                      int * arg_ax, int * arg_ay)
{
	int const min_x_dev  = input_abs_get_min(handle->dev, ABS_X);
	int const min_y_dev  = input_abs_get_min(handle->dev, ABS_Y);
	int const max_x_dev  = input_abs_get_max(handle->dev, ABS_X);
	int const max_y_dev  = input_abs_get_max(handle->dev, ABS_Y);
	int const max_y_norm = max_y_dev - min_y_dev;
	int const max_x_norm = max_x_dev - min_x_dev;

	if (!max_x_norm   || !max_y_norm   || !arg_ax       || !arg_ay ||
	    x < min_x_dev || y < min_y_dev || x > max_x_dev || y > max_y_dev) {
		printk("Ignore input source with coordinates out of range\n");
		return false;
	}

	*arg_ax = screen_x * (x - min_x_dev) / (max_x_norm);
	*arg_ay = screen_y * (y - min_y_dev) / (max_y_norm);

	return true;
}

void genode_evdev_event(struct input_handle *handle, unsigned int type,
                        unsigned int code, int value)
{
#if DEBUG_EVDEV
	static unsigned long count = 0;
#endif

	static int slot    =  0; /* store current input slot */

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

		/* don't generate press/release events for multitouch devices */
		if (code == BTN_TOUCH && !disable_multitouch)
			return;

		/* map BTN_TOUCH events to BTN_LEFT for absolute events */
		if (code == BTN_TOUCH && !disable_absolute)
			arg_keycode = BTN_LEFT;

		switch (value) {

		case 0:
			arg_type = EVENT_TYPE_RELEASE;
			break;

		case 1:
			arg_type = EVENT_TYPE_PRESS;
			break;

		default:
			printk("Unknown key event value %d - not handled\n", value);
			return;
		}
		break;

	case EV_ABS:
		switch (code) {

		case ABS_WHEEL:

			if (disable_absolute) return;

			arg_type = EVENT_TYPE_WHEEL;
			arg_ry = value;
			break;

		case ABS_X:
		case ABS_MT_POSITION_X:

			if (code == ABS_X             && disable_absolute)   return;
			if (code == ABS_MT_POSITION_X && disable_multitouch) return;

			if (((slots[slot].event == ABS_MT_POSITION_X) ||
			     (slots[slot].event == ABS_X)) && (slots[slot].y != -1)) {

				arg_keycode = slot;
				arg_type    = code == ABS_X ? EVENT_TYPE_MOTION : EVENT_TYPE_TOUCH;
				arg_ax      = slots[slot].x;
				arg_ay      = slots[slot].y;

				if (screen_x && screen_y &&
					!transform(handle, arg_ax, arg_ay, &arg_ax, &arg_ay))
					return;

				if (handler)
					handler(arg_type, arg_keycode, arg_ax, arg_ay, arg_rx, arg_ry);
			}

			slots[slot].event = code;

			/*
			 * Don't create an input event yet. Store the value and wait for the
			 * subsequent Y event.
			 */
			slots[slot].x = value;
			return;

		case ABS_Y:
		case ABS_MT_POSITION_Y:

			if (code == ABS_Y             && disable_absolute)   return;
			if (code == ABS_MT_POSITION_Y && disable_multitouch) return;

			slots[slot].event = code;
			slots[slot].y     = value;

			if (slots[slot].x == -1)
				return;

			/*
			 * Create a unified input event with absolute positions on x and y
			 * axis.
			 */
			arg_keycode = slot;
			arg_type    = code == ABS_Y ? EVENT_TYPE_MOTION : EVENT_TYPE_TOUCH;
			arg_ax      = slots[slot].x;
			arg_ay      = value;

			/* transform if requested */
			if (screen_x && screen_y &&
				!transform(handle, arg_ax, value, &arg_ax, &arg_ay))
				return;

			break;

		 case ABS_MT_TRACKING_ID:

			if (disable_multitouch) return;

			if (value != -1) {
				if (slots[slot].id != -1)
					lx_printf("warning:: old tracking id in use and got new one\n");

				slots[slot].id = value;
				return;
			}

			/* send end of slot usage event for clients */
			arg_keycode = slot;
			arg_type    = EVENT_TYPE_TOUCH;
			arg_ax      = slots[slot].x < 0 ? 0 : slots[slot].x;
			arg_ay      = slots[slot].y < 0 ? 0 : slots[slot].y;
			arg_rx      = arg_ry = -1;

			if (screen_x && screen_y)
				transform(handle, arg_ax, arg_ay, &arg_ax, &arg_ay);

			slots[slot].event = slots[slot].x = slots[slot].y = -1;
			slots[slot].id = value;

			break;

		case ABS_MT_SLOT:

			if (disable_multitouch) return;

			if (value >= sizeof(slots) / sizeof(slots[0])) {
				lx_printf("warning: drop slot id %d\n", value);
				return;
			}

			slot = value;
			return;

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

#if DEBUG_EVDEV
	printk("event[%ld]. dev: %s, type: %d, code: %d, value: %d a=%d,%d "
	       "r=%d,%d\n", count++, handle->dev->name, type, code, value, arg_ax,
	       arg_ay, arg_rx, arg_ry);
#endif
}


void genode_input_register(genode_input_event_cb h, unsigned long res_x,
                           unsigned long res_y, bool multitouch)
{
	unsigned i = 0;
	for (i = 0; i < sizeof(slots) / sizeof(slots[0]); i++)
		slots[i].id = slots[i].event = slots[i].x = slots[i].y = -1;

	handler = h;

	/* XXX make it per usb device configurable XXX */
	screen_x = res_x;
	screen_y = res_y;

	disable_multitouch = !multitouch;
	disable_absolute   = !disable_multitouch;
}
