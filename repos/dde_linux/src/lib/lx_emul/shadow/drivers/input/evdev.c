/*
 * \brief  Linux emulation environment: input event sink
 * \author Christian Helmuth
 * \date   2022-06-23
 *
 * This implementation is derived from drivers/input/evbug.c and
 * drivers/input/evdev.c.
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_emul.h>
#include <lx_emul/event.h>
#include <genode_c_api/event.h>

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/init.h>
#include <linux/device.h>

/*
 * Input devices with motion events
 *
 * (from Documentation/input/event-codes.rst and multi-touch-protocol.rst)
 *
 * The INPUT_PROP_DIRECT property indicates that device coordinates should be
 * directly mapped to screen coordinates (not taking into account trivial
 * transformations, such as scaling, flipping and rotating).
 * -> touchscreen, tablet (stylus/pen)
 *
 * Non-direct input devices may require non-trivial transformation, such as
 * absolute to relative transformation.
 * -> mouse, touchpad
 *
 * Historically a touch device with BTN_TOOL_FINGER and BTN_TOUCH was
 * interpreted as a touchpad by userspace, while a similar device without
 * BTN_TOOL_FINGER was interpreted as a touchscreen. For backwards
 * compatibility with current userspace it is recommended to follow this
 * distinction.
 *
 * In Linux, stylus/pen tool proximity is reported by BTN_TOOL_PEN/RUBBER plus
 * ABS_DISTANCE events. The actual contact to the surface emits an additional
 * BTN_TOUCH event. For multi-touch devices, the "tool" is also reported via
 * BTN_TOOL_FINGER/DOUBLETAP etc.
 *
 * Thus, these devices must be differentiated.
 *
 *   Mouse:       relative motion
 *   Pointer:     absolute motion (Qemu usb-tablet and IP-KVM devices)
 *   Touchpad:    relative motion via absolute touchpad coordinates
 *   Touchtool:   absolute motion (e.g., stylus)
 *   Touchscreen: absolute motion and finger (multi-) touch
 */

static bool is_rel_dev(struct input_dev *dev)
{
	return test_bit(EV_REL, dev->evbit) && test_bit(REL_X, dev->relbit);
}

static bool is_abs_dev(struct input_dev *dev)
{
	return test_bit(EV_ABS, dev->evbit) && test_bit(ABS_X, dev->absbit);
}

static bool is_touch_dev(struct input_dev *dev)
{
	return test_bit(BTN_TOUCH, dev->keybit);
}

static bool is_tool_dev(struct input_dev *dev)
{
	return test_bit(BTN_TOOL_PEN,      dev->keybit)
	    || test_bit(BTN_TOOL_RUBBER,   dev->keybit)
	    || test_bit(BTN_TOOL_BRUSH,    dev->keybit)
	    || test_bit(BTN_TOOL_PENCIL,   dev->keybit)
	    || test_bit(BTN_TOOL_AIRBRUSH, dev->keybit)
	    || test_bit(BTN_TOOL_MOUSE,    dev->keybit)
	    || test_bit(BTN_TOOL_LENS,     dev->keybit);
}

enum evdev_motion {
	MOTION_NONE,
	MOTION_MOUSE,       /* relative motion */
	MOTION_POINTER,     /* absolute motion */
	MOTION_TOUCHPAD,    /* relative motion based on absolute axes */
	MOTION_TOUCHTOOL,   /* absolute motion */
	MOTION_TOUCHSCREEN, /* absolute motion */
};

enum evdev_motion evdev_motion(struct input_dev const *dev)
{
	if (is_rel_dev(dev))
		return MOTION_MOUSE;

	if (!is_abs_dev(dev))
		return MOTION_NONE;

	if (!is_touch_dev(dev))
		return MOTION_POINTER;

	if (test_bit(BTN_TOOL_FINGER, dev->keybit))
		return MOTION_TOUCHPAD;

	if (is_tool_dev(dev))
		return MOTION_TOUCHTOOL;

	return MOTION_TOUCHSCREEN;
}


struct evdev_mt_slot
{
	int id; /* -1 means unused */
	int x, y, ox, oy;
};
#define INIT_MT_SLOT (struct evdev_mt_slot){ -1, -1, -1, -1, -1 }


/*
 * Maximum number of touch slots supported.
 *
 * Many Linux drivers report 2 to 10 slots, the Magic Trackpad reports 16. The
 * Surface driver reports 64, which we just ignore.
 */
enum { MAX_MT_SLOTS = 16 };

struct evdev_mt
{
	bool                 pending;
	unsigned             num_slots;
	unsigned             cur_slot;
	struct evdev_mt_slot slots[MAX_MT_SLOTS];
};

#define array_for_each_element(element, array) \
	for ((element) = (array); \
	     (element) < ((array) + ARRAY_SIZE((array))); \
	     (element)++)

#define for_each_mt_slot(slot, mt) \
	array_for_each_element(slot, (mt)->slots)


struct evdev_key
{
	bool     pending;
	unsigned code;
	bool     press;

	typeof(jiffies) jiffies;
};
#define INIT_KEY (struct evdev_key){ false, 0, false }
#define EVDEV_KEY(code, press) (struct evdev_key){ true, code, press, jiffies }

struct evdev_keys
{
	unsigned         pending; /* pending keys counter */
	struct evdev_key key[16]; /* max 16 keys per packet */
};

#define for_each_key(key, keys, pending_only) \
	array_for_each_element(key, (keys)->key) \
		if (!(pending_only) || (key)->pending)

#define for_each_pending_key(key, keys) \
	if ((keys)->pending) \
		for_each_key(key, keys, true)


struct evdev_xy
{
	bool pending;
	int  x, y;
};
#define INIT_XY (struct evdev_xy){ false, 0, 0 }


struct evdev_touchpad
{
	typeof(jiffies) touch_time;
	bool            btn_left_pressed; /* state of (physical) BTN_LEFT */
};


struct evdev
{
	struct genode_event *event;
	struct input_handle  handle;
	enum evdev_motion    motion;

	/* record of all events in one packet - submitted on SYN */
	unsigned          tool;  /* BTN_TOOL_* or 0 */
	struct evdev_keys keys;
	struct evdev_xy   rel;
	struct evdev_xy   wheel;
	struct evdev_xy   abs;
	struct evdev_mt   mt;

	/* device-specific state machine */
	union {
		struct evdev_touchpad touchpad;
	};
};


/* helper functions (require declarations above) */
#include "evdev.h"


static bool record_mouse(struct evdev *evdev, struct input_value const *v)
{
	if (v->type != EV_REL || evdev->motion != MOTION_MOUSE)
		return false;

	switch (v->code) {
	case REL_X:      evdev->rel.pending   = true; evdev->rel.x   += v->value; break;
	case REL_Y:      evdev->rel.pending   = true; evdev->rel.y   += v->value; break;
	case REL_HWHEEL: evdev->wheel.pending = true; evdev->wheel.x += v->value; break;
	case REL_WHEEL:  evdev->wheel.pending = true; evdev->wheel.y += v->value; break;

	default:
		return false;
	}

	return true;
}


static bool record_abs(struct evdev *evdev, struct input_value const *v)
{
	if (v->type != EV_ABS)
		return false;

	switch (v->code) {
	case ABS_X:      evdev->abs.pending   = true; evdev->abs.x    = v->value; break;
	case ABS_Y:      evdev->abs.pending   = true; evdev->abs.y    = v->value; break;

	default:
		return false;
	}

	return true;
}


static bool record_wheel(struct evdev *evdev, struct input_value const *v)
{
	if (v->type != EV_REL)
		return false;

	switch (v->code) {
	case REL_HWHEEL: evdev->wheel.pending = true; evdev->wheel.x += v->value; break;
	case REL_WHEEL:  evdev->wheel.pending = true; evdev->wheel.y += v->value; break;

	default:
		return false;
	}

	return true;
}


static bool record_pointer(struct evdev *evdev, struct input_value const *v)
{
	if (evdev->motion != MOTION_POINTER)
		return false;

	return record_abs(evdev, v) || record_wheel(evdev, v);
}


static bool record_touchtool(struct evdev *evdev, struct input_value const *v)
{
	if (evdev->motion != MOTION_TOUCHTOOL)
		return false;

	return record_abs(evdev, v) || record_wheel(evdev, v);
}


static bool record_mt(struct evdev_mt *mt, struct input_value const *v)
{
	if (v->type != EV_ABS || !mt->num_slots)
		return false;

	switch (v->code) {
	case ABS_MT_SLOT:
		mt->cur_slot = (v->value >= 0 ? v->value : 0);
		/* nothing pending yet */
		break;

	case ABS_MT_TRACKING_ID:
		if (mt->cur_slot < mt->num_slots) {
			mt->slots[mt->cur_slot].id = v->value >= 0 ? mt->cur_slot : -1;
			mt->pending = true;
		}
		break;

	case ABS_MT_POSITION_X:
		if (mt->cur_slot < mt->num_slots) {
			mt->slots[mt->cur_slot].x = v->value;
			mt->pending = true;
		}
		break;

	case ABS_MT_POSITION_Y:
		if (mt->cur_slot < mt->num_slots) {
			mt->slots[mt->cur_slot].y = v->value;
			mt->pending = true;
		}
		break;

	default:
		return false;
	}

	return true;
}


static bool record_touchpad(struct evdev *evdev, struct input_value const *v)
{
	if (evdev->motion != MOTION_TOUCHPAD)
		return false;

	/* monitor (physical) button state clashing with tap-to-click */
	if (v->type == EV_KEY && v->code == BTN_LEFT)
		evdev->touchpad.btn_left_pressed = !!v->value;

	/* only multi-touch pads supported currently */
	return record_mt(&evdev->mt, v);
}


static bool record_touchscreen(struct evdev *evdev, struct input_value const *v)
{
	if (evdev->motion != MOTION_TOUCHSCREEN)
		return false;

	/* only multi-touch screens supported currently */
	return record_mt(&evdev->mt, v);
}


static bool is_tool_key(unsigned code)
{
	switch (code) {
	case BTN_TOOL_PEN:
	case BTN_TOOL_RUBBER:
	case BTN_TOOL_BRUSH:
	case BTN_TOOL_PENCIL:
	case BTN_TOOL_AIRBRUSH:
	case BTN_TOOL_FINGER:
	case BTN_TOOL_MOUSE:
	case BTN_TOOL_LENS:
	case BTN_TOOL_QUINTTAP:
	case BTN_TOOL_DOUBLETAP:
	case BTN_TOOL_TRIPLETAP:
	case BTN_TOOL_QUADTAP:
		return true;

	default:
		return false;
	}
}

static bool record_key(struct evdev *evdev, struct input_value const *v)
{
	struct evdev_keys * const keys = &evdev->keys;

	if (v->type != EV_KEY)
		return false;

	/* silently drop KEY_FN as hardware switch */
	if (v->code == KEY_FN)
		return true;

	if (is_tool_key(v->code)) {
		evdev->tool = v->value ? v->code : 0;
	} else {
		struct evdev_key *key;
		for_each_key(key, keys, false) {
			if (key->pending)
				continue;

			*key = EVDEV_KEY(v->code, !!v->value);
			keys->pending++;
			break;
		}
	}

	return true;
}


static void submit_press_release(struct evdev_key *key, struct evdev_keys *keys,
                                 struct genode_event_submit *submit)
{
	if (!key->pending)
		return;

	if (key->press)
		submit->press(submit, lx_emul_event_keycode(key->code));
	else
		submit->release(submit, lx_emul_event_keycode(key->code));

	*key = INIT_KEY;
	keys->pending--;
}


static void submit_keys(struct evdev_keys *keys, struct genode_event_submit *submit)
{
	struct evdev_key *key;

	if (!keys->pending)
		return;

	for_each_pending_key(key, keys)
		submit_press_release(key, keys, submit);
}


static void submit_mouse(struct evdev *evdev, struct genode_event_submit *submit)
{
	if (evdev->motion != MOTION_MOUSE)
		return;

	if (evdev->rel.pending) {
		submit->rel_motion(submit, evdev->rel.x, evdev->rel.y);
		evdev->rel = INIT_XY;
	}

	if (evdev->wheel.pending) {
		submit->wheel(submit, evdev->wheel.x, evdev->wheel.y);
		evdev->wheel = INIT_XY;
	}
}


static void submit_pointer(struct evdev *evdev, struct genode_event_submit *submit)
{
	if (evdev->motion != MOTION_POINTER)
		return;

	if (evdev->abs.pending) {
		submit->abs_motion(submit, evdev->abs.x, evdev->abs.y);
		evdev->abs.pending = false;
	}

	if (evdev->wheel.pending) {
		submit->wheel(submit, evdev->wheel.x, evdev->wheel.y);
		evdev->wheel = INIT_XY;
	}
}


static void submit_touchtool(struct evdev *evdev, struct genode_event_submit *submit)
{
	struct evdev_keys * const keys = &evdev->keys;

	struct evdev_key *key;

	if (evdev->motion != MOTION_TOUCHTOOL)
		return;

	if (evdev->abs.pending) {
		submit->abs_motion(submit, evdev->abs.x, evdev->abs.y);
		evdev->abs.pending = false;
	}

	/* submit recorded tool on BTN_TOUCH */
	for_each_pending_key(key, keys) {
		if (key->code != BTN_TOUCH)
			continue;

		key->code = evdev->tool;
		submit_press_release(key, keys, submit);
		break;
	}
}


static void touchpad_tap_to_click(struct evdev_keys *keys, struct evdev_touchpad *tp,
                                  struct genode_event_submit *submit)
{
	enum { TAP_TIME = 130 /* max touch duration in ms */ };

	struct evdev_key *key;

	if (!keys->pending)
		return;

	for_each_pending_key(key, keys) {
		if (key->code != BTN_TOUCH)
			continue;

		if (key->press && !tp->btn_left_pressed) {
			tp->touch_time = key->jiffies;
		} else {
			if (time_before(key->jiffies, tp->touch_time + msecs_to_jiffies(TAP_TIME))) {
				submit->press(submit, lx_emul_event_keycode(BTN_LEFT));
				submit->release(submit, lx_emul_event_keycode(BTN_LEFT));
			}
			tp->touch_time = 0;
		}

		*key = INIT_KEY;
		keys->pending--;
		break;
	}
}


static void submit_touchpad(struct evdev *evdev, struct genode_event_submit *submit)
{
	struct evdev_mt * const mt = &evdev->mt;

	struct evdev_mt_slot *slot;

	if (evdev->motion != MOTION_TOUCHPAD)
		return;

	/*
	 * TODO device state model
	 *
	 * - click without small motion (if pad is pressable button)
	 * - two-finger scrolling
	 * - edge scrolling
	 * - virtual-button regions
	 *
	 * https://wayland.freedesktop.org/libinput/doc/latest/tapping.html
	 */

	if (mt->pending) {
		for_each_mt_slot(slot, mt) {
			if (slot->id == -1) {
				*slot = INIT_MT_SLOT;
				continue;
			}

			if (slot->ox != -1 && slot->oy != -1)
				submit->rel_motion(submit, slot->x - slot->ox, slot->y - slot->oy);

			slot->ox = slot->x;
			slot->oy = slot->y;
		}

		mt->pending = false;
	}

	touchpad_tap_to_click(&evdev->keys, &evdev->touchpad, submit);
}


static void submit_touchscreen(struct evdev *evdev, struct genode_event_submit *submit)
{
	struct evdev_mt * const mt = &evdev->mt;
	struct evdev_keys * const keys = &evdev->keys;

	struct evdev_key *key;
	struct evdev_mt_slot *slot;

	if (evdev->motion != MOTION_TOUCHSCREEN)
		return;

	if (mt->pending) {
		for_each_mt_slot(slot, mt) {
			if (slot->id == -1 && slot->ox != -1 && slot->oy != -1) {
				submit->touch_release(submit, slot->id);

				*slot = INIT_MT_SLOT;
				continue;
			}

			/* skip unchanged slots */
			if (slot->ox == slot->x && slot->oy == slot->y)
				continue;

			if (slot->x != -1 && slot->y != -1) {
				struct genode_event_touch_args args = {
					.finger = slot->id,
					.xpos   = slot->x,
					.ypos   = slot->y,
					.width  = 1
				};
				submit->touch(submit, &args);
			}

			slot->ox = slot->x;
			slot->oy = slot->y;
		}

		mt->pending = false;
	}

	/* filter BTN_TOUCH */
	for_each_pending_key(key, keys) {
		if (key->code != BTN_TOUCH)
			continue;

		*key = INIT_KEY;
		keys->pending--;
		break;
	}
}


static bool submit_on_syn(struct evdev *evdev, struct input_value const *v,
                          struct genode_event_submit *submit)
{
	if (v->type != EV_SYN || v->code != SYN_REPORT)
		return false;

	/* motion devices */
	submit_mouse(evdev, submit);
	submit_pointer(evdev, submit);
	submit_touchpad(evdev, submit);
	submit_touchtool(evdev, submit);
	submit_touchscreen(evdev, submit);

	/* submit keys not handled above */
	submit_keys(&evdev->keys, submit);

	return true;
}


struct genode_event_generator_ctx
{
	struct evdev *evdev;
	struct input_value const *values;
	unsigned count;
};


static void evdev_event_generator(struct genode_event_generator_ctx *ctx,
                                  struct genode_event_submit *submit)
{
	for (int i = 0; i < ctx->count; i++) {
		struct evdev *evdev = ctx->evdev;

		struct input_value const *v = &ctx->values[i];

		bool processed = false;

		/* filter injected EV_LED updates */
		if (v->type == EV_LED) continue;

		/* filter input_repeat_key() */
		if ((v->type == EV_KEY) && (v->value > 1)) continue;

		processed |= record_mouse(evdev, v);
		processed |= record_pointer(evdev, v);
		processed |= record_touchpad(evdev, v);
		processed |= record_touchtool(evdev, v);
		processed |= record_touchscreen(evdev, v);
		processed |= record_key(evdev, v);
		processed |= submit_on_syn(evdev, v, submit);

		if (!processed)
			printk("Dropping unsupported Event[%u/%u] device=%s type=%s code=%s value=%d\n",
			       i + 1, ctx->count, evdev->handle.dev->name,
			       NAME_OF_TYPE(v->type), NAME_OF_CODE(v->type, v->code), v->value);
	}
}


static void evdev_events(struct input_handle *handle,
                         struct input_value const *values, unsigned int count)
{
	struct evdev *evdev = handle->private;

	struct genode_event_generator_ctx ctx = {
		.evdev = evdev, .values = values, .count = count };

	genode_event_generate(evdev->event, &evdev_event_generator, &ctx);
}


static void evdev_event(struct input_handle *handle,
                        unsigned int type, unsigned int code, int value)
{
	struct input_value vals[] = { { type, code, value } };

	evdev_events(handle, vals, 1);
}


static void init_motion(struct evdev *evdev)
{
	struct input_dev *dev = evdev->handle.dev;

	evdev->motion = evdev_motion(dev);

	switch (evdev->motion) {
	case MOTION_NONE:
	case MOTION_MOUSE:
	case MOTION_POINTER:
		/* nothing to do */
		break;

	case MOTION_TOUCHPAD:
	case MOTION_TOUCHTOOL:
	case MOTION_TOUCHSCREEN:
		if (dev->mt) {
			struct evdev_mt *mt = &evdev->mt;

			struct evdev_mt_slot *slot;

			mt->num_slots = min(dev->mt->num_slots, MAX_MT_SLOTS);
			mt->cur_slot = 0;
			for_each_mt_slot(slot, mt)
				*slot = INIT_MT_SLOT;

			/* disable undesired events */
			clear_bit(ABS_X,              dev->absbit);
			clear_bit(ABS_Y,              dev->absbit);
			clear_bit(ABS_PRESSURE,       dev->absbit);
			clear_bit(ABS_MT_TOUCH_MAJOR, dev->absbit);
			clear_bit(ABS_MT_TOUCH_MINOR, dev->absbit);
			clear_bit(ABS_MT_WIDTH_MAJOR, dev->absbit);
			clear_bit(ABS_MT_WIDTH_MINOR, dev->absbit);
			clear_bit(ABS_MT_ORIENTATION, dev->absbit);
			clear_bit(ABS_MT_TOOL_TYPE,   dev->absbit);
			clear_bit(ABS_MT_PRESSURE,    dev->absbit);
			clear_bit(ABS_MT_TOOL_X,      dev->absbit);
			clear_bit(ABS_MT_TOOL_Y,      dev->absbit);
		} else {
			/* disable undesired events */
			clear_bit(ABS_PRESSURE,       dev->absbit);
			clear_bit(ABS_DISTANCE,       dev->absbit);
		}
		break;
	}
}


static int evdev_connect(struct input_handler *handler, struct input_dev *dev,
                         const struct input_device_id *id)
{
	struct evdev *evdev;
	int error;
	struct genode_event_args args = { .label = dev->name };

	evdev = kzalloc(sizeof(*evdev), GFP_KERNEL);
	if (!evdev)
		return -ENOMEM;

	evdev->event = genode_event_create(&args);

	evdev->handle.private = evdev;
	evdev->handle.dev     = dev;
	evdev->handle.handler = handler;
	evdev->handle.name    = dev->name;

	init_motion(evdev);

	/* disable undesired events */
	clear_bit(EV_MSC,            dev->evbit);
	clear_bit(REL_HWHEEL_HI_RES, dev->relbit);
	clear_bit(REL_WHEEL_HI_RES,  dev->relbit);

	error = input_register_handle(&evdev->handle);
	if (error)
		goto err_free_handle;

	error = input_open_device(&evdev->handle);
	if (error)
		goto err_unregister_handle;

	printk("Connected device: %s (%s at %s) %s%s\n",
	       dev_name(&dev->dev),
	       dev->name ?: "unknown",
	       dev->phys ?: "unknown",
	       dev->mt ? "MULTITOUCH " : "",
	       evdev->motion != MOTION_NONE ? NAME_OF_MOTION(evdev->motion) : "");

	return 0;

err_unregister_handle:
	input_unregister_handle(&evdev->handle);

err_free_handle:
	genode_event_destroy(evdev->event);
	kfree(evdev);
	return error;
}


static void evdev_disconnect(struct input_handle *handle)
{
	struct evdev *evdev = handle->private;

	printk("Disconnected device: %s\n", dev_name(&handle->dev->dev));

	input_close_device(handle);
	input_unregister_handle(handle);
	genode_event_destroy(evdev->event);
	kfree(evdev);
}


static const struct input_device_id evdev_ids[] = {
	{ .driver_info = 1 }, /* Matches all devices */
	{ },                  /* Terminating zero entry */
};


MODULE_DEVICE_TABLE(input, evdev_ids);

static struct input_handler evdev_handler = {
	.event      = evdev_event,
	.events     = evdev_events,
	.connect    = evdev_connect,
	.disconnect = evdev_disconnect,
	.name       = "evdev",
	.id_table   = evdev_ids,
};


static int __init evdev_init(void)
{
	return input_register_handler(&evdev_handler);
}


static void __exit evdev_exit(void)
{
	input_unregister_handler(&evdev_handler);
}


/**
 * Let's hook into the evdev initcall, so we do not need to register
 * an additional one
 */
module_init(evdev_init);
module_exit(evdev_exit);
