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
 * TODO differentiate touchpads, trackpads, touchscreen, etc.
 *
 * (from Documentation/input/event-codes.rst)
 *
 * INPUT_PROP_DIRECT + INPUT_PROP_POINTER
 * --------------------------------------
 *
 * The INPUT_PROP_DIRECT property indicates that device coordinates should be
 * directly mapped to screen coordinates (not taking into account trivial
 * transformations, such as scaling, flipping and rotating). Non-direct input
 * devices require non-trivial transformation, such as absolute to relative
 * transformation for touchpads. Typical direct input devices: touchscreens,
 * drawing tablets; non-direct devices: touchpads, mice.
 *
 * The INPUT_PROP_POINTER property indicates that the device is not transposed
 * on the screen and thus requires use of an on-screen pointer to trace user's
 * movements.  Typical pointer devices: touchpads, tablets, mice; non-pointer
 * device: touchscreen.
 *
 * If neither INPUT_PROP_DIRECT or INPUT_PROP_POINTER are set, the property is
 * considered undefined and the device type should be deduced in the
 * traditional way, using emitted event types.
 */

struct evdev_mt_slot
{
	int tracking_id; /* -1 means unused */
	int x, y, ox, oy;
};
#define INIT_MT_SLOT (struct evdev_mt_slot){ -1, -1, -1, -1, -1 }

/* just stay with primary and secondary touch for now */
enum { MAX_MT_SLOTS = 2, PRIMARY = 0, SECONDARY = 1, };

struct evdev_mt
{
	unsigned             num_slots;
	unsigned             cur_slot;
	struct evdev_mt_slot slots[MAX_MT_SLOTS];
};

struct evdev
{
	struct genode_event *event;
	struct input_handle  handle;
	unsigned             pending;
	int                  rel_x, rel_y, rel_wx, rel_wy;
	struct evdev_mt      mt;
};


struct genode_event_generator_ctx
{
	struct evdev *evdev;
	struct input_value const *values;
	unsigned count;
};


struct name { char s[32]; };
#define NAME_INIT(name, fmt, ...) snprintf(name.s, sizeof(name.s), fmt, ## __VA_ARGS__)


static struct name name_of_type(unsigned type)
{
	struct name result = { { 0 } };

	switch (type) {
	case EV_SYN: NAME_INIT(result, "SYN"); break;
	case EV_KEY: NAME_INIT(result, "KEY"); break;
	case EV_REL: NAME_INIT(result, "REL"); break;
	case EV_ABS: NAME_INIT(result, "ABS"); break;
	case EV_MSC: NAME_INIT(result, "MSC"); break;
	default:     NAME_INIT(result, "%3u", type);
	}

	return result;
}
#define NAME_OF_TYPE(type) name_of_type(type).s


static struct name name_of_code(unsigned type, unsigned code)
{
	struct name result = { { 0 } };

	switch (type) {
	case EV_SYN:
		switch (code) {
		case SYN_REPORT: NAME_INIT(result, "REPORT"); break;

		default: NAME_INIT(result, "%u", code);
		} break;

	case EV_KEY:
		switch (code) {
		case BTN_LEFT:           NAME_INIT(result, "BTN_LEFT");           break;
		case BTN_RIGHT:          NAME_INIT(result, "BTN_RIGHT");          break;
		case BTN_TOOL_FINGER:    NAME_INIT(result, "BTN_TOOL_FINGER");    break;
		case BTN_TOUCH:          NAME_INIT(result, "BTN_TOUCH");          break;
		case BTN_TOOL_DOUBLETAP: NAME_INIT(result, "BTN_TOOL_DOUBLETAP"); break;
		case BTN_TOOL_TRIPLETAP: NAME_INIT(result, "BTN_TOOL_TRIPLETAP"); break;
		case BTN_TOOL_QUADTAP:   NAME_INIT(result, "BTN_TOOL_QUADTAP");   break;
		case BTN_TOOL_QUINTTAP:  NAME_INIT(result, "BTN_TOOL_QUINTTAP");  break;

		default: NAME_INIT(result, "%u", code);
		} break;

	case EV_REL:
		switch (code) {
		case REL_X:      NAME_INIT(result, "X");      break;
		case REL_Y:      NAME_INIT(result, "Y");      break;
		case REL_HWHEEL: NAME_INIT(result, "HWHEEL"); break;
		case REL_WHEEL:  NAME_INIT(result, "WHEEL");  break;
		case REL_MISC:   NAME_INIT(result, "MISC");   break;

		default: NAME_INIT(result, "%u", code);
		} break;

	case EV_ABS:
		switch (code) {
		case ABS_X:              NAME_INIT(result, "X");              break;
		case ABS_Y:              NAME_INIT(result, "Y");              break;
		case ABS_MISC:           NAME_INIT(result, "MISC");           break;
		case ABS_MT_SLOT:        NAME_INIT(result, "MT_SLOT");        break;
		case ABS_MT_POSITION_X:  NAME_INIT(result, "MT_POSITION_X");  break;
		case ABS_MT_POSITION_Y:  NAME_INIT(result, "MT_POSITION_Y");  break;
		case ABS_MT_TOOL_TYPE:   NAME_INIT(result, "MT_TOOL_TYPE");   break;
		case ABS_MT_TRACKING_ID: NAME_INIT(result, "MT_TRACKING_ID"); break;

		default: NAME_INIT(result, "%u", code);
		} break;

	case EV_MSC:
		switch (code) {
		case MSC_SCAN:      NAME_INIT(result, "SCAN");      break;
		case MSC_TIMESTAMP: NAME_INIT(result, "TIMESTAMP"); break;

		default: NAME_INIT(result, "%u", code);
		} break;

	default:     NAME_INIT(result, "%u", code);
	}

	return result;
}
#define NAME_OF_CODE(type, code) name_of_code(type, code).s


static bool handle_key(struct evdev *evdev, struct input_value const *v,
                       struct genode_event_submit *submit)
{
	unsigned code = v->code;

	if (v->type != EV_KEY)
		return false;

	/* map BTN_TOUCH to BTN_LEFT */
	if (code == BTN_TOUCH) code = BTN_LEFT;

	if (v->value)
		submit->press(submit, lx_emul_event_keycode(code));
	else
		submit->release(submit, lx_emul_event_keycode(code));

	return true;
}


static bool record_rel(struct evdev *evdev, struct input_value const *v)
{
	if (v->type != EV_REL)
		return false;

	switch (v->code) {
	case REL_X:      evdev->rel_x  += v->value; break;
	case REL_Y:      evdev->rel_y  += v->value; break;
	case REL_HWHEEL: evdev->rel_wx += v->value; break;
	case REL_WHEEL:  evdev->rel_wy += v->value; break;

	default:
		return false;
	}
	evdev->pending++;

	return true;
}


static bool record_abs(struct evdev *evdev, struct input_value const *v)
{
	struct evdev_mt * const mt = &evdev->mt;

	if (v->type != EV_ABS)
		return false;

	if (mt->num_slots) {
		switch (v->code) {
		case ABS_MT_SLOT:
			mt->cur_slot = (v->value >= 0 ? v->value : 0);
			break;

		case ABS_MT_TRACKING_ID:
			if (mt->cur_slot < mt->num_slots) {
				mt->slots[mt->cur_slot] = INIT_MT_SLOT;
				evdev->pending++;
			}
			break;

		case ABS_MT_POSITION_X:
			if (mt->cur_slot < mt->num_slots) {
				mt->slots[mt->cur_slot].x = v->value;
				evdev->pending++;
			}
			break;

		case ABS_MT_POSITION_Y:
			if (mt->cur_slot < mt->num_slots) {
				mt->slots[mt->cur_slot].y = v->value;
				evdev->pending++;
			}
			break;

		default:
			return false;
		}
	} else {
		/* XXX absolute events not supported currently */
		return false;
	}

	return true;
}


static void submit_mt_motion(struct evdev_mt_slot       *slot,
                             struct genode_event_submit *submit)
{
	if (slot->ox != -1 && slot->oy != -1)
		submit->rel_motion(submit, slot->x - slot->ox, slot->y - slot->oy);

	slot->ox = slot->x;
	slot->oy = slot->y;
}


static bool submit_on_syn(struct evdev *evdev, struct input_value const *v,
                          struct genode_event_submit *submit)
{
	struct evdev_mt * const mt = &evdev->mt;

	if (v->type != EV_SYN || v->code != SYN_REPORT)
		return false;
	if (!evdev->pending)
		return true;

	if (mt->num_slots) {
		submit_mt_motion(&mt->slots[PRIMARY],   submit);
		submit_mt_motion(&mt->slots[SECONDARY], submit);
	} else {
		if (evdev->rel_x || evdev->rel_y) {
			submit->rel_motion(submit, evdev->rel_x, evdev->rel_y);
			evdev->rel_x = evdev->rel_y = 0;
		}
		if (evdev->rel_wx || evdev->rel_wy) {
			submit->wheel(submit, evdev->rel_wx, evdev->rel_wy);
			evdev->rel_wx = evdev->rel_wy = 0;
		}
		/* XXX absolute events not supported currently */
	}

	evdev->pending = 0;

	return true;
}

static void evdev_event_generator(struct genode_event_generator_ctx *ctx,
                                  struct genode_event_submit *submit)
{
	for (int i = 0; i < ctx->count; i++) {
		struct evdev *evdev = ctx->evdev;

		struct input_value const *v = &ctx->values[i];

		bool processed = false;

		/* filter input_repeat_key() */
		if ((v->type == EV_KEY) && (v->value > 1)) continue;

		processed |= handle_key(evdev, v, submit);
		processed |= record_abs(evdev, v);
		processed |= record_rel(evdev, v);
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

	if (dev->mt) {
		struct evdev_mt *mt = &evdev->mt;

		mt->num_slots = min(dev->mt->num_slots, MAX_MT_SLOTS);
		mt->cur_slot = 0;
		for (int i = 0; i < sizeof(mt->slots)/sizeof(*mt->slots); i++)
			mt->slots[i] = INIT_MT_SLOT;

		/* disable undesired events */
		clear_bit(ABS_MT_TOOL_TYPE,   dev->absbit);
		clear_bit(ABS_X,              dev->absbit);
		clear_bit(ABS_Y,              dev->absbit);
		clear_bit(BTN_TOOL_FINGER,    dev->keybit);
		clear_bit(BTN_TOOL_DOUBLETAP, dev->keybit);
		clear_bit(BTN_TOOL_TRIPLETAP, dev->keybit);
		clear_bit(BTN_TOOL_QUADTAP,   dev->keybit);
		clear_bit(BTN_TOOL_QUINTTAP,  dev->keybit);
		clear_bit(BTN_TOUCH,          dev->keybit);
	}

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

	printk("Connected device: %s (%s at %s)\n",
	       dev_name(&dev->dev),
	       dev->name ?: "unknown",
	       dev->phys ?: "unknown");

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
