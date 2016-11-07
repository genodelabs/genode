/*
 * \brief  Input service and event handler
 * \author Christian Helmuth
 * \author Dirk Vogt
 * \author Sebastian Sumpf
 * \author Christian Menard
 * \author Alexander Boettcher
 * \date   2009-04-20
 *
 * TODO make this a complete replacement for evdev.c from Linux
 * TODO per-device slot handling
 *
 * The original implementation was in the L4Env from the TUD:OS group
 * (l4/pkg/input/lib/src/l4evdev.c). This file was released under the terms of
 * the GNU General Public License version 2.
 */

/*
 * Copyright (C) 2009-2016 Genode Labs GmbH
 * Copyright (C) 2014      Ksys Labs LLC
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/log.h>

/* Linux includes */
#include <lx_emul.h>
#include <lx_emul/extern_c_begin.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <lx_emul/extern_c_end.h>


/* Callback function to Genode subsystem */
static genode_input_event_cb handler;

static unsigned long screen_x;
static unsigned long screen_y;

/*
 * We only send multi-touch events if enabled. Otherwise emulated pointer events
 * are generated.
 */
static bool multi_touch;

static struct slot
{
	int id;    /* current tracking id  */
	int x;     /* last reported x axis */
	int y;     /* last reported y axis */
	int event; /* last reported ABS_MT_ event */
} slots[16];

static int slot = 0; /* store current input slot */


static bool transform(input_dev *dev, int &x, int &y)
{
	if (!screen_x || !screen_y) return true;

	int const min_x_dev  = input_abs_get_min(dev, ABS_X);
	int const min_y_dev  = input_abs_get_min(dev, ABS_Y);
	int const max_x_dev  = input_abs_get_max(dev, ABS_X);
	int const max_y_dev  = input_abs_get_max(dev, ABS_Y);
	int const max_y_norm = max_y_dev - min_y_dev;
	int const max_x_norm = max_x_dev - min_x_dev;

	if (!max_x_norm   || !max_y_norm   ||
	    x < min_x_dev || y < min_y_dev || x > max_x_dev || y > max_y_dev) {
		Genode::warning("ignore input source with coordinates out of range");
		return false;
	}

	x = screen_x * (x - min_x_dev) / (max_x_norm);
	y = screen_y * (y - min_y_dev) / (max_y_norm);

	return true;
}


static void handle_mt_tracking_id(input_dev *dev, int value)
{
	if (value != -1) {
		if (slots[slot].id != -1)
			Genode::warning("old tracking id in use and got new one");

		slots[slot].id = value;
		return;
	}

	/* send end of slot usage event for clients */
	int x = slots[slot].x < 0 ? 0 : slots[slot].x;
	int y = slots[slot].y < 0 ? 0 : slots[slot].y;

	if (!transform(dev, x, y)) return;

	if (handler)
		handler(EVENT_TYPE_TOUCH, slot, x, y, -1, -1);

	slots[slot].event = slots[slot].x = slots[slot].y = -1;
	slots[slot].id = value;
}


static void handle_mt_slot(int value)
{
	if ((unsigned)value >= sizeof(slots) / sizeof(slots[0])) {
		Genode::warning("drop multi-touch slot id ", value);
		return;
	}

	slot = value;
}


enum Axis { AXIS_X, AXIS_Y };

static void handle_absolute_axis(input_dev *dev, unsigned code, int value, Axis axis)
{
	slots[slot].event = code;

	input_event_type type;

	switch (axis) {
	case AXIS_X:
		type          = code == ABS_X ? EVENT_TYPE_MOTION : EVENT_TYPE_TOUCH;
		slots[slot].x = value;
		break;
	case AXIS_Y:
		type          = code == ABS_Y ? EVENT_TYPE_MOTION : EVENT_TYPE_TOUCH;
		slots[slot].y = value;
		break;
	}

	int x = slots[slot].x;
	int y = slots[slot].y;

	if (x == -1 || y == -1) return;

	if (!transform(dev, x, y)) return;

	if (handler)
		handler(type, slot, x, y, 0, 0);
}


static void handle_absolute(input_dev *dev, unsigned code, int value)
{
	switch (code) {
	case ABS_WHEEL:
		if (handler)
			handler(EVENT_TYPE_WHEEL, 0, 0, 0, 0, value);
		return;

	case ABS_X:
		if (dev->mt && multi_touch) return;
		handle_absolute_axis(dev, code, value, AXIS_X);
		return;

	case ABS_MT_POSITION_X:
		if (!multi_touch) return;
		handle_absolute_axis(dev, code, value, AXIS_X);
		return;

	case ABS_Y:
		if (dev->mt && multi_touch) return;
		handle_absolute_axis(dev, code, value, AXIS_Y);
		return;

	case ABS_MT_POSITION_Y:
		if (!multi_touch) return;
		handle_absolute_axis(dev, code, value, AXIS_Y);
		return;

	case ABS_MT_TRACKING_ID:
		if (!multi_touch) return;
		handle_mt_tracking_id(dev, value);
		return;

	case ABS_MT_SLOT:
		if (!multi_touch) return;
		handle_mt_slot(value);
		return;

	case ABS_MT_TOUCH_MAJOR:
	case ABS_MT_TOUCH_MINOR:
	case ABS_MT_ORIENTATION:
	case ABS_MT_TOOL_TYPE:
	case ABS_MT_BLOB_ID:
	case ABS_MT_PRESSURE:
	case ABS_MT_DISTANCE:
	case ABS_MT_TOOL_X:
	case ABS_MT_TOOL_Y:
		/* ignore unused multi-touch events */
		return;

	default:
		Genode::warning("unknown absolute event code ", code, " not handled");
		return;
	}
}


static void handle_relative(unsigned code, int value)
{
	input_event_type type;
	int x = 0, y = 0;

	switch (code) {
	case REL_X:
		type = EVENT_TYPE_MOTION;
		x    = value;
		break;

	case REL_Y:
		type = EVENT_TYPE_MOTION;
		y    = value;
		break;

	case REL_HWHEEL:
		type = EVENT_TYPE_WHEEL;
		x    = value;
		break;

	case REL_WHEEL:
		type = EVENT_TYPE_WHEEL;
		y    = value;
		break;

	default:
		Genode::warning("unknown relative event code ", code, " not handled");
		return;
	}

	if (handler)
		handler(type, 0, 0, 0, x, y);
}


static void handle_key(input_dev *dev, unsigned code, int value)
{
	/* no press/release events for multi-touch devices in multi-touch mode */
	if (dev->mt && multi_touch) return;

	/* map BTN_TOUCH to BTN_LEFT */
	if (code == BTN_TOUCH) code = BTN_LEFT;

	input_event_type type;
	switch (value) {
	case 0: type = EVENT_TYPE_RELEASE; break;
	case 1: type = EVENT_TYPE_PRESS;   break;

	default:
		Genode::warning("unknown key event value ", value, " not handled");
		return;
	}

	if (handler)
		handler(type, code, 0, 0, 0, 0);
}


extern "C" void genode_evdev_event(input_handle *handle, unsigned int type,
                                   unsigned int code, int value)
{
	input_dev *dev = handle->dev;

	/* filter sound events */
	if (test_bit(EV_SND, handle->dev->evbit)) return;

	/* filter input_repeat_key() */
	if ((type == EV_KEY) && (value == 2)) return;

	/* filter EV_SYN and EV_MSC */
	if (type == EV_SYN || type == EV_MSC) return;

	switch (type) {
	case EV_KEY: handle_key(dev, code, value);      return;
	case EV_REL: handle_relative(code, value);      return;
	case EV_ABS: handle_absolute(dev, code, value); return;

	default:
		Genode::warning("unknown event type ", type, " not handled");
		return;
	}
}


void genode_input_register(genode_input_event_cb h, unsigned long res_x,
                           unsigned long res_y, bool multitouch)
{
	for (unsigned i = 0; i < sizeof(slots)/sizeof(*slots); ++i)
		slots[i].id = slots[i].event = slots[i].x = slots[i].y = -1;

	handler     = h;
	screen_x    = res_x;
	screen_y    = res_y;
	multi_touch = multitouch;
}
