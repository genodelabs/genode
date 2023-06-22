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
#include <linux/init.h>
#include <linux/device.h>


struct evdev
{
	struct genode_event *event;
	struct input_handle  handle;
};


static void submit_rel_motion(struct genode_event_submit *submit,
                              unsigned code, int value)
{
	switch (code) {
	case REL_X:      submit->rel_motion(submit, value, 0); break;
	case REL_Y:      submit->rel_motion(submit, 0, value); break;

	case REL_HWHEEL: submit->wheel(submit, value, 0);      break;
	case REL_WHEEL:  submit->wheel(submit, 0, value);      break;

	/* skip for now because of large values */
	case REL_HWHEEL_HI_RES: break;
	case REL_WHEEL_HI_RES:  break;

	default:
		printk("Unsupported relative motion event code=%d dropped\n", code);
	}
}


static void submit_key(struct genode_event_submit *submit,
                       unsigned code, bool press)
{
	/* map BTN_TOUCH to BTN_LEFT */
	if (code == BTN_TOUCH) code = BTN_LEFT;

	if (press)
		submit->press(submit, lx_emul_event_keycode(code));
	else
		submit->release(submit, lx_emul_event_keycode(code));
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
	int i;

	for (i = 0; i < ctx->count; i++) {
		unsigned const type  = ctx->values[i].type;
		unsigned const code  = ctx->values[i].code;
		unsigned const value = ctx->values[i].value;

		/* filter input_repeat_key() */
		if ((type == EV_KEY) && (value > 1)) continue;

		/* filter EV_SYN and EV_MSC */
		if (type == EV_SYN || type == EV_MSC) continue;

		switch (type) {
		case EV_KEY: submit_key(submit, code, value);        break;
		case EV_REL: submit_rel_motion(submit, code, value); break;

		default:
			printk("Unsupported Event[%u/%u] device=%s, type=%d, code=%d, value=%d dropped\n",
			       i + 1, ctx->count, dev_name(&ctx->evdev->handle.dev->dev), type, code, value);
			continue;
		}
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
