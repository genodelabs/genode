/*
 * \brief  Inject input event into an X server
 * \author Norman Feske
 * \date   2009-11-04
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* local includes */
#include "inject_input.h"

/* Genode includes */
#include <input/keycodes.h>
#include <base/printf.h>

/* X11 includes */
#include <X11/extensions/XTest.h>


static int convert_keycode_to_x11[] = {
	  0,   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  13,  14,  15,
	 16,  17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,  29,  30,  31,
	 32,  33,  34,  35,  36,  37,  38,  39,  40,  41,  42,  43,  44,  45,  46,  47,
	 48,  49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,  62,  63,
	 64,  65,  66,  67,  68,  69,  70,  71,  72,  73,  74,  75,  76,  77,  78,  79,
	 80,  81,  82,  83,  43,  85,  86,  87,  88, 115, 119, 120, 121, 375, 123,  90,
	284, 285, 309, 298, 312,  91, 327, 328, 329, 331, 333, 335, 336, 337, 338, 339,
	367, 294, 293, 286, 350,  92, 334, 512, 116, 377, 109, 111, 373, 347, 348, 349,
	360,  93,  94,  95,  98, 376, 100, 101, 357, 316, 354, 304, 289, 102, 351, 355,
	103, 104, 105, 275, 281, 272, 306, 106, 274, 107, 288, 364, 358, 363, 362, 361,
	291, 108, 381, 290, 287, 292, 279, 305, 280,  99, 112, 257, 258, 113, 270, 114,
	118, 117, 125, 374, 379, 259, 260, 261, 262, 263, 264, 265, 266, 267, 268, 269,
	271, 273, 276, 277, 278, 282, 283, 295, 296, 297, 299, 300, 301, 302, 303, 307,
	308, 310, 313, 314, 315, 317, 318, 319, 320, 321, 322, 323, 324, 325, 326, 330,
	332, 340, 341, 342, 343, 344, 345, 346, 356, 359, 365, 368, 369, 370, 371, 372
};


static int convert_keycode(int keycode)
{
	if (keycode < 0 || keycode >= (int)(sizeof(convert_keycode_to_x11)/sizeof(int)))
		return 0;

	return convert_keycode_to_x11[keycode] + 8;
}


/**********************
 ** Public functions **
 **********************/

bool inject_input_init(Display *dpy)
{
	int dummy;
	if (!XTestQueryExtension (dpy, &dummy, &dummy, &dummy, &dummy)) {
		Genode::printf ("Error: Could not query Xtest extension\n");
		return false;
	}
	return true;
}


void inject_input_event(Display *dpy, Input::Event &ev)
{
	switch (ev.type()) {

	case Input::Event::MOTION:
		XTestFakeMotionEvent(dpy, -1, ev.ax(), ev.ay(), CurrentTime);
		break;

	case Input::Event::PRESS:
		if (ev.code() == Input::BTN_LEFT)
			XTestFakeButtonEvent(dpy, 1, 1, CurrentTime);
		if (ev.code() == Input::BTN_RIGHT)
			XTestFakeButtonEvent(dpy, 2, 1, CurrentTime);
		else
			XTestFakeKeyEvent(dpy, convert_keycode(ev.code()), 1, CurrentTime);
		break;

	case Input::Event::RELEASE:
		if (ev.code() == Input::BTN_LEFT)
			XTestFakeButtonEvent(dpy, 1, 0, CurrentTime);
		if (ev.code() == Input::BTN_RIGHT)
			XTestFakeButtonEvent(dpy, 2, 0, CurrentTime);
		else
			XTestFakeKeyEvent(dpy, convert_keycode(ev.code()), 0, CurrentTime);
		break;

	default: break;
	}

	/* flush faked input events */
	XFlush(dpy);
}
