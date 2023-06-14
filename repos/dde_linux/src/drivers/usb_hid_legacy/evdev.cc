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
 * Copyright (C) 2009-2017 Genode Labs GmbH
 * Copyright (C) 2014      Ksys Labs LLC
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/log.h>
#include <base/registry.h>
#include <util/reconstructible.h>

/* LX kit */
#include <legacy/lx_kit/env.h>
#include <legacy/lx_kit/scheduler.h>

/* local */
#include "led_state.h"

/* Linux includes */
#include <driver.h>
#include <lx_emul.h>
#include <legacy/lx_emul/extern_c_begin.h>
#include <linux/hid.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/usb.h>
#include <legacy/lx_emul/extern_c_end.h>


static struct slot
{
	int id    = -1; /* current tracking id  */
	int x     = -1; /* last reported x axis */
	int y     = -1; /* last reported y axis */
	int event = -1; /* last reported ABS_MT_ event */
} slots[16];

static int slot = 0; /* store current input slot */


static bool transform(input_dev *dev, int &x, int &y)
{
	if (!Driver::screen_x || !Driver::screen_y) return true;

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

	x = Driver::screen_x * (x - min_x_dev) / (max_x_norm);
	y = Driver::screen_y * (y - min_y_dev) / (max_y_norm);

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

	Driver::input_callback(Driver::EVENT_TYPE_TOUCH, slot, x, y, -1, -1);

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

	Driver::Input_event type = Driver::EVENT_TYPE_MOTION;

	switch (axis) {
	case AXIS_X:
		type          = code == ABS_X ? Driver::EVENT_TYPE_MOTION
		                              : Driver::EVENT_TYPE_TOUCH;
		slots[slot].x = value;
		break;
	case AXIS_Y:
		type          = code == ABS_Y ? Driver::EVENT_TYPE_MOTION
		                              : Driver::EVENT_TYPE_TOUCH;
		slots[slot].y = value;
		break;
	}

	int x = slots[slot].x;
	int y = slots[slot].y;

	if (x == -1 || y == -1) return;

	if (!transform(dev, x, y)) return;

	Driver::input_callback(type, slot, x, y, 0, 0);
}


static void handle_absolute(input_dev *dev, unsigned code, int value)
{
	switch (code) {
	case ABS_WHEEL:
		Driver::input_callback(Driver::EVENT_TYPE_WHEEL, 0, 0, 0, 0, value);
		return;

	case ABS_X:
		if (dev->mt && Driver::multi_touch) return;
		handle_absolute_axis(dev, code, value, AXIS_X);
		return;

	case ABS_MT_POSITION_X:
		if (!Driver::multi_touch) return;
		handle_absolute_axis(dev, code, value, AXIS_X);
		return;

	case ABS_Y:
		if (dev->mt && Driver::multi_touch) return;
		handle_absolute_axis(dev, code, value, AXIS_Y);
		return;

	case ABS_MT_POSITION_Y:
		if (!Driver::multi_touch) return;
		handle_absolute_axis(dev, code, value, AXIS_Y);
		return;

	case ABS_MT_TRACKING_ID:
		if (!Driver::multi_touch) return;
		handle_mt_tracking_id(dev, value);
		return;

	case ABS_MT_SLOT:
		if (!Driver::multi_touch) return;
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
	Driver::Input_event type;
	int x = 0, y = 0;

	switch (code) {
	case REL_X:
		type = Driver::EVENT_TYPE_MOTION;
		x    = value;
		break;

	case REL_Y:
		type = Driver::EVENT_TYPE_MOTION;
		y    = value;
		break;

	case REL_HWHEEL:
		type = Driver::EVENT_TYPE_WHEEL;
		x    = value;
		break;

	case REL_WHEEL:
		type = Driver::EVENT_TYPE_WHEEL;
		y    = value;
		break;

	default:
		Genode::warning("unknown relative event code ", code, " not handled");
		return;
	}

	Driver::input_callback(type, 0, 0, 0, x, y);
}


static void handle_key(input_dev *dev, unsigned code, int value)
{
	/* no press/release events for multi-touch devices in multi-touch mode */
	if (dev->mt && Driver::multi_touch) return;

	/* map BTN_TOUCH to BTN_LEFT */
	if (code == BTN_TOUCH) code = BTN_LEFT;

	Driver::Input_event type;
	switch (value) {
	case 0: type = Driver::EVENT_TYPE_RELEASE; break;
	case 1: type = Driver::EVENT_TYPE_PRESS;   break;

	default:
		Genode::warning("unknown key event value ", value, " not handled");
		return;
	}

	Driver::input_callback(type, code, 0, 0, 0, 0);
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


/***************************
 ** Keyboard LED handling **
 ***************************/

class Keyboard_led
{
	private:

		Genode::Registry<Keyboard_led>::Element _reg_elem;
		input_dev * const                       _input_dev;

		usb_interface *_interface() {
			return container_of(_input_dev->dev.parent->parent, usb_interface, dev); }

		usb_device *_usb_device() {
			return interface_to_usbdev(_interface()); }

	public:

		Keyboard_led(Genode::Registry<Keyboard_led> &registry, input_dev *dev)
		: _reg_elem(registry, *this), _input_dev(dev) { }

		bool match(input_dev const *other) const { return _input_dev == other; }

		void update(unsigned leds)
		{
			unsigned *buf = (unsigned *)kmalloc(4, GFP_LX_DMA);
			*buf = leds;
			usb_control_msg(_usb_device(), usb_sndctrlpipe(_usb_device(), 0),
			                0x9, USB_TYPE_CLASS  | USB_RECIP_INTERFACE, 0x200,
			                _interface()->cur_altsetting->desc.bInterfaceNumber,
			                buf, 1, 500);
			kfree(buf);
		}
};


static Genode::Registry<Keyboard_led> _registry;


namespace Usb { class Led; }

class Usb::Led
{
	private:

		Lx::Task _task { _run, this, "led_worker", Lx::Task::PRIORITY_2,
		                 Lx::scheduler() };

		completion _config_update;
		completion _led_update;

		enum Update_state { NONE, UPDATE, BLOCKED };
		Update_state _update_state { NONE };

		Led_state _capslock { Lx_kit::env().env(), "capslock" },
		          _numlock  { Lx_kit::env().env(), "numlock"  },
		          _scrlock  { Lx_kit::env().env(), "scrlock"  };

		Genode::Signal_handler<Led> _config_handler {
			Lx_kit::env().env().ep(), *this, &Led::_handle_config };

		void _handle_config()
		{
			Lx_kit::env().config_rom().update();
			Genode::Xml_node config = Lx_kit::env().config_rom().xml();

			_capslock.update(config, _config_handler);
			_numlock .update(config, _config_handler);
			_scrlock .update(config, _config_handler);

			complete(&_config_update);
			Lx::scheduler().schedule();
		}

		static void _run(void *l)
		{
			Led *led = (Led *)l;

			while (true) {
				/* config update from EP */
				wait_for_completion(&led->_config_update);

				led->_update_state = UPDATE;

				_registry.for_each([&] (Keyboard_led &keyboard) {
					led->update(keyboard); });

				/* wake up other task that waits for regestry access */
				if (led->_update_state == BLOCKED)
					complete(&led->_led_update);

				led->_update_state = NONE;
			}
		}

	public:

		Led()
		{
			init_completion(&_config_update);
			init_completion(&_led_update);
			Genode::Signal_transmitter(_config_handler).submit();
		}

		void update(Keyboard_led &keyboard)
		{
			unsigned leds = 0;

			leds |= _capslock.enabled() ? 1u << LED_CAPSL   : 0;
			leds |= _numlock.enabled()  ? 1u << LED_NUML    : 0;
			leds |= _scrlock.enabled()  ? 1u << LED_SCROLLL : 0;

			keyboard.update(leds);
		}

		/*
		 * wait for completion of registry and led state updates
		 */
		void wait_for_registry()
		{
			/* task in _run function might receive multiple updates */
			while ((_update_state == UPDATE)) {
				_update_state = BLOCKED;
				wait_for_completion(&_led_update);
			}
		}
};


static Genode::Constructible<Usb::Led> _led;


static int led_connect(struct input_handler *handler, struct input_dev *dev,
                       const struct input_device_id *id)
{
	_led->wait_for_registry();

	Keyboard_led *keyboard = new (Lx_kit::env().heap()) Keyboard_led(_registry, dev);
	_led->update(*keyboard);

	input_handle *handle = (input_handle *)kzalloc(sizeof(input_handle), 0);
	handle->dev     = input_get_device(dev);
	handle->handler = handler;

	input_register_handle(handle);

	return 0;
}


static void led_disconnect(struct input_handle *handle)
{
	input_dev *dev = handle->dev;

	_led->wait_for_registry();

	_registry.for_each([&] (Keyboard_led &keyboard) {
		if (keyboard.match(dev))
			destroy(Lx_kit::env().heap(), &keyboard);
	});

	input_unregister_handle(handle);
	input_put_device(dev);
	kfree(handle);
}


static bool led_match(struct input_handler *handler, struct input_dev *dev)
{
	hid_device *hid = (hid_device *)input_get_drvdata(dev);
	hid_report *report;

	/* search report for keyboard entries */
	list_for_each_entry(report, &hid->report_enum[0].report_list, list) {

		for (unsigned i = 0; i < report->maxfield; i++)
			for (unsigned j = 0; j < report->field[i]->maxusage; j++) {
				hid_usage *usage = report->field[i]->usage + j;
				if ((usage->hid & HID_USAGE_PAGE) == HID_UP_KEYBOARD) {
					return true;
				}
			}
	}

	return false;
}


static struct input_handler   led_handler;
static struct input_device_id led_ids[2];

static int led_init(void)
{
	led_ids[0].driver_info = 1; /* match all */
	led_ids[1] = {};

	led_handler.name       = "led";
	led_handler.connect    = led_connect;
	led_handler.disconnect = led_disconnect;
	led_handler.id_table   = led_ids;
	led_handler.match      = led_match;

	_led.construct();

	return input_register_handler(&led_handler);
}


extern "C" { module_init(led_init); }
