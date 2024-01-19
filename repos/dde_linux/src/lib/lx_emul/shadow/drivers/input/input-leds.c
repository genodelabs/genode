/*
 * \brief  Input LED handling
 * \author Sebastian Sumpf
 * \author Christian Helmuth
 * \date   2023-06-29
 *
 * This implementation is derived from drivers/input/input-leds.c.
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <linux/hid.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/usb.h>

#include <lx_emul/input_leds.h>


struct led_handler
{
	struct list_head    list;
	struct input_handle handle;
};

static LIST_HEAD(led_handlers);


enum Update_state { NONE, UPDATE, BLOCKED };

struct led_update
{
	enum Update_state state;
	struct completion update;

	bool capsl, numl, scrolll;
};

static struct led_update led_update;


static void update_leds(struct led_handler *handler)
{
	input_inject_event(&handler->handle, EV_LED, LED_CAPSL,   led_update.capsl);
	input_inject_event(&handler->handle, EV_LED, LED_NUML,    led_update.numl);
	input_inject_event(&handler->handle, EV_LED, LED_SCROLLL, led_update.scrolll);
}


void lx_emul_input_leds_update(bool capslock, bool numlock, bool scrolllock)
{

	struct led_handler *handler;

	led_update.state = UPDATE;

	led_update.capsl   = capslock;
	led_update.numl    = numlock;
	led_update.scrolll = scrolllock;

	list_for_each_entry(handler, &led_handlers, list) {
		update_leds(handler);
	}

	if (led_update.state == BLOCKED)
		complete(&led_update.update);

	led_update.state = NONE;
}


static void wait_for_update(void)
{
	if (led_update.state == UPDATE) {
		led_update.state = BLOCKED;
		wait_for_completion(&led_update.update);
	}
}


static int input_leds_connect(struct input_handler *input_handler, struct input_dev *dev,
                              const struct input_device_id *id)
{
	struct led_handler *handler;

	wait_for_update();

	handler = (struct led_handler *)kzalloc(sizeof(*handler), GFP_KERNEL);
	if (!handler)
		return -ENOMEM;

	handler->handle.dev     = input_get_device(dev);
	handler->handle.handler = input_handler;
	handler->handle.name    = "leds";
	handler->handle.private = handler;

	INIT_LIST_HEAD(&handler->list);
	list_add_tail(&handler->list, &led_handlers);

	update_leds(handler);

	input_register_handle(&handler->handle);

	return 0;
}


static void input_leds_disconnect(struct input_handle *handle)
{
	struct led_handler *handler = (struct led_handler *)handle->private;

	wait_for_update();

	list_del(&handler->list);
	input_unregister_handle(handle);
	input_put_device(handle->dev);
	kfree(handler);
}


static struct input_device_id input_leds_ids[] = {
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT,
		.evbit = { BIT_MASK(EV_LED) },
	},
};


MODULE_DEVICE_TABLE(input, led_ids);

static struct input_handler input_leds_handler = {
	.name       = "input-leds",
	.connect    = input_leds_connect,
	.disconnect = input_leds_disconnect,
	.id_table   = input_leds_ids,
};


static int __init input_leds_init(void)
{
	led_update.state = NONE;
	init_completion(&led_update.update);

	return input_register_handler(&input_leds_handler);
}


static void __exit input_leds_exit(void)
{
	input_unregister_handler(&input_leds_handler);
}


module_init(input_leds_init);
module_exit(input_leds_exit);
