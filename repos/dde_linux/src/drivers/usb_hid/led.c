/*
 * \brief  Keyboard LED handling
 * \author Sebastian Sumpf
 * \date   2023-06-29
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

struct keyboard
{
	struct input_dev     *input_dev;
	struct usb_interface *intf;
	struct usb_device    *udev;
	struct list_head      list;
};


enum Update_state { NONE, UPDATE, BLOCKED };

struct led_update
{
	enum Update_state state;
	struct completion update;
	unsigned          leds;
};


static LIST_HEAD(_keyboards);
static struct led_update _led_update;


static bool keyboard_match(struct keyboard *kbd, struct input_dev *input_dev)
{
	return kbd->input_dev == input_dev;
}


static void keyboard_update(struct keyboard *kbd, unsigned leds)
{
	usb_control_msg(kbd->udev, usb_sndctrlpipe(kbd->udev, 0),
	                0x9, USB_TYPE_CLASS  | USB_RECIP_INTERFACE, 0x200,
	                kbd->intf->cur_altsetting->desc.bInterfaceNumber,
	                &leds, 1, 500);
}


void lx_led_state_update(bool capslock, bool numlock, bool scrlock)
{

	struct keyboard *kbd;
	unsigned leds = 0;

	leds |= capslock ? 1u << LED_CAPSL   : 0;
	leds |= numlock  ? 1u << LED_NUML    : 0;
	leds |= scrlock  ? 1u << LED_SCROLLL : 0;

	_led_update.leds  = leds;
	_led_update.state = UPDATE;

	/* udpdate keyboards */
	list_for_each_entry(kbd, &_keyboards, list) {
		keyboard_update(kbd, leds);
	}

	if (_led_update.state == BLOCKED)
		complete(&_led_update.update);

	_led_update.state = NONE;
}


static void wait_for_update(void)
{
	if (_led_update.state == UPDATE) {
		_led_update.state = BLOCKED;
		wait_for_completion(&_led_update.update);
	}
}


static int led_connect(struct input_handler *handler, struct input_dev *dev,
                       const struct input_device_id *id)
{
	struct keyboard     *kbd;
	struct input_handle *handle;

	wait_for_update();

	handle = (struct input_handle *)kzalloc(sizeof(*handle), 0);
	if (!handle) return -ENOMEM;
	handle->dev     = input_get_device(dev);
	handle->handler = handler;

	kbd = (struct keyboard *)kzalloc(sizeof(*kbd), GFP_KERNEL);
	if (!kbd) goto err;

	kbd->input_dev = input_get_device(dev);
	kbd->intf      = container_of(kbd->input_dev->dev.parent->parent, struct usb_interface, dev);
	kbd->udev      = interface_to_usbdev(kbd->intf);

	INIT_LIST_HEAD(&kbd->list);
	list_add_tail(&kbd->list, &_keyboards);

	keyboard_update(kbd, _led_update.leds);

	input_register_handle(handle);

	return 0;

err:
	kfree(handle);
	return -ENOMEM;
}


static void led_disconnect(struct input_handle *handle)
{
	struct input_dev *dev = handle->dev;
	struct keyboard  *kbd, *temp;

	wait_for_update();

	list_for_each_entry_safe(kbd, temp, &_keyboards, list) {
		if (keyboard_match(kbd, dev)) {
			list_del(&kbd->list);
			kfree(kbd);
		}
	}
	input_unregister_handle(handle);
	input_put_device(dev);
	kfree(handle);
}


static bool led_match(struct input_handler *handler, struct input_dev *dev)
{
	struct hid_device *hid = (struct hid_device *)input_get_drvdata(dev);
	struct hid_report *report;
	struct hid_usage  *usage;
	unsigned i, j;

	/* search report for keyboard entries */
	list_for_each_entry(report, &hid->report_enum[0].report_list, list) {

		for (i = 0; i < report->maxfield; i++)
			for (j = 0; j < report->field[i]->maxusage; j++) {
				usage = report->field[i]->usage + j;
				if ((usage->hid & HID_USAGE_PAGE) == HID_UP_KEYBOARD) {
					return true;
				}
			}
	}

	return false;
}


static struct input_device_id led_ids[] = {
	{ .driver_info = 1 }, /* match all */
	{ },
};


MODULE_DEVICE_TABLE(input, led_ids);

static struct input_handler led_handler = {
	.name       = "keyboard_led",
	.connect    = led_connect,
	.disconnect = led_disconnect,
	.id_table   = led_ids,
	.match      = led_match,
	.id_table   = led_ids,
};


static int __init input_leds_init(void)
{
	_led_update.state = NONE;
	init_completion(&_led_update.update);

	return input_register_handler(&led_handler);
}


static void __exit input_leds_exit(void)
{
	input_unregister_handler(&led_handler);
}


/**
 * Let's hook into the input_leds initcall, so we do not need to register
 * an additional one
 */
module_init(input_leds_init);
module_exit(input_leds_exit);
