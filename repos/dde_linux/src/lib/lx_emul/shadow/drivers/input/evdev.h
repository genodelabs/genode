/*
 * \brief  Linux emulation environment: evdev helpers
 * \author Christian Helmuth
 * \date   2024-01-29
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _SHADOW__DRIVERS__INPUT__EVDEV_H_
#define _SHADOW__DRIVERS__INPUT__EVDEV_H_


struct name { char s[32]; };
#define NAME_INIT(name, fmt, ...) snprintf(name.s, sizeof(name.s), fmt, ## __VA_ARGS__)

static struct name name_of_motion(enum evdev_motion motion)
{
	struct name result = { { 0 } };

	switch (motion) {
	case MOTION_NONE:        NAME_INIT(result, "NONE");        break;
	case MOTION_MOUSE:       NAME_INIT(result, "MOUSE");       break;
	case MOTION_POINTER:     NAME_INIT(result, "POINTER");     break;
	case MOTION_TOUCHPAD:    NAME_INIT(result, "TOUCHPAD");    break;
	case MOTION_TOUCHTOOL:   NAME_INIT(result, "TOUCHTOOL");   break;
	case MOTION_TOUCHSCREEN: NAME_INIT(result, "TOUCHSCREEN"); break;
	}

	return result;
}
#define NAME_OF_MOTION(type) name_of_motion(type).s

static struct name name_of_prop(unsigned prop)
{
	struct name result = { { 0 } };

	switch (prop) {
	case INPUT_PROP_POINTER:        NAME_INIT(result, "POINTER");        break;
	case INPUT_PROP_DIRECT:         NAME_INIT(result, "DIRECT");         break;
	case INPUT_PROP_BUTTONPAD:      NAME_INIT(result, "BUTTONPAD");      break;
	case INPUT_PROP_SEMI_MT:        NAME_INIT(result, "SEMI_MT");        break;
	case INPUT_PROP_TOPBUTTONPAD:   NAME_INIT(result, "TOPBUTTONPAD");   break;
	case INPUT_PROP_POINTING_STICK: NAME_INIT(result, "POINTING_STICK"); break;
	case INPUT_PROP_ACCELEROMETER:  NAME_INIT(result, "ACCELEROMETER");  break;
	default:                        NAME_INIT(result, "%2u", prop);
	}

	return result;
}
#define NAME_OF_PROP(type) name_of_prop(type).s

static struct name name_of_type(unsigned type)
{
	struct name result = { { 0 } };

	switch (type) {
	case EV_SYN: NAME_INIT(result, "SYN"); break;
	case EV_KEY: NAME_INIT(result, "KEY"); break;
	case EV_REL: NAME_INIT(result, "REL"); break;
	case EV_ABS: NAME_INIT(result, "ABS"); break;
	case EV_MSC: NAME_INIT(result, "MSC"); break;
	case EV_SW:  NAME_INIT(result, "SW "); break;
	case EV_LED: NAME_INIT(result, "LED"); break;
	case EV_REP: NAME_INIT(result, "REP"); break;
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
		case BTN_MIDDLE:         NAME_INIT(result, "BTN_MIDDLE");         break;
		case BTN_SIDE:           NAME_INIT(result, "BTN_SIDE");           break;
		case BTN_EXTRA:          NAME_INIT(result, "BTN_EXTRA");          break;
		case BTN_FORWARD:        NAME_INIT(result, "BTN_FORWARD");        break;
		case BTN_BACK:           NAME_INIT(result, "BTN_BACK");           break;
		case BTN_TASK:           NAME_INIT(result, "BTN_TASK");           break;
		case BTN_TOOL_PEN:       NAME_INIT(result, "BTN_TOOL_PEN");       break;
		case BTN_TOOL_RUBBER:    NAME_INIT(result, "BTN_TOOL_RUBBER");    break;
		case BTN_TOOL_FINGER:    NAME_INIT(result, "BTN_TOOL_FINGER");    break;
		case BTN_TOUCH:          NAME_INIT(result, "BTN_TOUCH");          break;
		case BTN_STYLUS:         NAME_INIT(result, "BTN_STYLUS");         break;
		case BTN_STYLUS2:        NAME_INIT(result, "BTN_STYLUS2");        break;
		case BTN_TOOL_DOUBLETAP: NAME_INIT(result, "BTN_TOOL_DOUBLETAP"); break;
		case BTN_TOOL_TRIPLETAP: NAME_INIT(result, "BTN_TOOL_TRIPLETAP"); break;
		case BTN_TOOL_QUADTAP:   NAME_INIT(result, "BTN_TOOL_QUADTAP");   break;
		case BTN_TOOL_QUINTTAP:  NAME_INIT(result, "BTN_TOOL_QUINTTAP");  break;

		default: NAME_INIT(result, "%u", code);
		} break;

	case EV_REL:
		switch (code) {
		case REL_X:             NAME_INIT(result, "X");             break;
		case REL_Y:             NAME_INIT(result, "Y");             break;
		case REL_HWHEEL:        NAME_INIT(result, "HWHEEL");        break;
		case REL_WHEEL:         NAME_INIT(result, "WHEEL");         break;
		case REL_MISC:          NAME_INIT(result, "MISC");          break;
		case REL_WHEEL_HI_RES:  NAME_INIT(result, "WHEEL_HI_RES");  break;
		case REL_HWHEEL_HI_RES: NAME_INIT(result, "HWHEEL_HI_RES"); break;

		default: NAME_INIT(result, "%u", code);
		} break;

	case EV_ABS:
		switch (code) {
		case ABS_X:              NAME_INIT(result, "X");              break;
		case ABS_Y:              NAME_INIT(result, "Y");              break;
		case ABS_PRESSURE:       NAME_INIT(result, "PRESSURE");       break;
		case ABS_DISTANCE:       NAME_INIT(result, "DISTANCE");       break;
		case ABS_MISC:           NAME_INIT(result, "MISC");           break;
		case ABS_MT_SLOT:        NAME_INIT(result, "MT_SLOT");        break;
		case ABS_MT_TOUCH_MAJOR: NAME_INIT(result, "MT_TOUCH_MAJOR"); break;
		case ABS_MT_TOUCH_MINOR: NAME_INIT(result, "MT_TOUCH_MINOR"); break;
		case ABS_MT_WIDTH_MAJOR: NAME_INIT(result, "MT_WIDTH_MAJOR"); break;
		case ABS_MT_WIDTH_MINOR: NAME_INIT(result, "MT_WIDTH_MINOR"); break;
		case ABS_MT_ORIENTATION: NAME_INIT(result, "MT_ORIENTATION"); break;
		case ABS_MT_POSITION_X:  NAME_INIT(result, "MT_POSITION_X");  break;
		case ABS_MT_POSITION_Y:  NAME_INIT(result, "MT_POSITION_Y");  break;
		case ABS_MT_TOOL_TYPE:   NAME_INIT(result, "MT_TOOL_TYPE");   break;
		case ABS_MT_TRACKING_ID: NAME_INIT(result, "MT_TRACKING_ID"); break;
		case ABS_MT_PRESSURE:    NAME_INIT(result, "MT_PRESSURE");    break;
		case ABS_MT_DISTANCE:    NAME_INIT(result, "MT_DISTANCE");    break;
		case ABS_MT_TOOL_X:      NAME_INIT(result, "MT_TOOL_X");      break;
		case ABS_MT_TOOL_Y:      NAME_INIT(result, "MT_TOOL_Y");      break;

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

static void log_event_in_packet(unsigned cur, unsigned max,
                                struct input_value const *v,
                                struct evdev const *evdev)
{
	printk("--- Event[%u/%u] %s '%s' %s type=%s code=%s value=%d\n", cur, max,
	       dev_name(&evdev->handle.dev->dev), evdev->handle.dev->name, NAME_OF_MOTION(evdev->motion),
	       NAME_OF_TYPE(v->type), NAME_OF_CODE(v->type, v->code), v->value);
}

static void log_device_info(struct input_dev *dev)
{
	unsigned bit;

	printk("device: %s (%s at %s)\n", dev_name(&dev->dev), dev->name ?: "unknown", dev->phys ?: "unknown");
	printk(" propbit:");
	for_each_set_bit(bit, dev->propbit, INPUT_PROP_CNT)
		printk(" %s", NAME_OF_PROP(bit));
	printk("\n");
	printk(" evbit:  ");
	for_each_set_bit(bit, dev->evbit, EV_CNT)
		printk(" %s", NAME_OF_TYPE(bit));
	printk("\n");

	if (test_bit(EV_REL, dev->evbit)) {
		printk(" relbit: ");
		for_each_set_bit(bit, dev->relbit, REL_CNT)
			printk(" %s", NAME_OF_CODE(EV_REL, bit));
		printk("\n");
	}
	if (test_bit(EV_ABS, dev->evbit)) {
		printk(" absbit: ");
		for_each_set_bit(bit, dev->absbit, ABS_CNT)
			printk(" %s", NAME_OF_CODE(EV_ABS, bit));
		printk("\n");
	}
	if (test_bit(EV_ABS, dev->evbit) || test_bit(EV_REL, dev->evbit)) {
		printk(" keybit: ");
		for_each_set_bit(bit, dev->keybit, KEY_CNT)
			printk(" %s", NAME_OF_CODE(EV_KEY, bit));
		printk("\n");
	}
	if (test_bit(EV_LED, dev->evbit)) {
		unsigned count = 0;
		for_each_set_bit(bit, dev->ledbit, LED_CNT)
			count++;
		printk(" leds:   %u\n", count);
	}
	printk(" hint_events_per_packet: %u max_vals: %u\n", dev->hint_events_per_packet, dev->max_vals);
	if (dev->mt) {
		printk(" dev->mt->flags=%x\n", dev->mt->flags);
		printk(" dev->mt->num_slots=%u\n", dev->mt->num_slots);
	}
	if (dev->absinfo) {
		printk(" absinfo: %px\n", dev->absinfo);
	}
}

#endif /* _SHADOW__DRIVERS__INPUT__EVDEV_H_ */
