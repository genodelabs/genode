/*
 * \brief  Fiasco-UX Input driver
 * \author Christian Helmuth
 * \date   2006-08-30
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/env.h>
#include <base/stdint.h>
#include <base/printf.h>
#include <base/snprintf.h>

#include <rom_session/connection.h>
#include <io_mem_session/connection.h>

namespace Fiasco {
#include <l4/sys/vhw.h>
#include <l4/input/libinput.h>

	void log_event(struct l4input *ev);
}

#include "input.h"

using namespace Genode;


/**
 * Input area
 */
static struct Fiasco::l4input *input_area;

static off_t  input_pos;
static size_t input_size = (1 << 12) / sizeof(struct Fiasco::l4input);


/****************
 ** Driver API **
 ****************/

bool Input_drv::event_pending()
{
	return (input_area[input_pos].time);
}

Input::Event Input_drv::get_event()
{
	using namespace Input;


	/* XXX we cannot block here without working IRQ handling - so return
	 * invalid event in case of empty buffer */
	if (!input_area[input_pos].time)
		return Event();

	/* read event */
	struct Fiasco::l4input ev = input_area[input_pos];

	/* mark slot as free */
	input_area[input_pos].time = 0;

	/* increment position counter */
	input_pos = (input_pos + 1) % input_size;

	/* determine event properties */
	Event::Type type;
	int keycode=0, abs_x=0, abs_y=0, rel_x=0, rel_y=0;
	switch (ev.type) {

	case EV_KEY:
		keycode = ev.code;
		if (ev.value)
			type = Event::PRESS;
		else
			type = Event::RELEASE;
		break;

	case EV_REL:
		switch (ev.code) {
		case REL_X:
		case REL_RX:
			type = Event::MOTION;
			rel_x = ev.value;
			break;

		case REL_Y:
		case REL_RY:
			type = Event::MOTION;
			rel_y = ev.value;
			break;

		case REL_WHEEL:
		case REL_HWHEEL:
			type = Event::WHEEL;
			rel_x = ev.value;
			break;

		default:
			type = Event::INVALID;
		}
		break;

	case EV_ABS:
		switch (ev.code) {
		case ABS_X:
		case ABS_RX:
			type = Event::MOTION;
			abs_x = ev.value;
			break;

		case ABS_Y:
		case ABS_RY:
			type = Event::MOTION;
			abs_y = ev.value;
			break;

		case ABS_WHEEL:
			type = Event::WHEEL;
			abs_x = ev.value;
			break;

		default:
			type = Event::INVALID;
		}
		break;

	default:
		type = Event::INVALID;
	}

	return Event(type, keycode, abs_x, abs_y, rel_x, rel_y);
}


/********************
 ** Driver startup **
 ********************/

/**
 * Configure Fiasco kernel info page
 */
static void *map_kip()
{
	/* request KIP dataspace */
	Rom_connection rom("l4v2_kip");
	rom.on_destruction(Rom_connection::KEEP_OPEN);

	/* attach KIP dataspace */
	return env()->rm_session()->attach(rom.dataspace());
}


/**
 * Read virtual hardware descriptor from kernel info page
 */
static int init_input_vhw(void *kip, addr_t *base, size_t *size)
{
	Fiasco::l4_kernel_info_t *kip_ptr = (Fiasco::l4_kernel_info_t *)kip;
	struct Fiasco::l4_vhw_descriptor *vhw = Fiasco::l4_vhw_get(kip_ptr);
	if (!vhw) return -1;

	struct Fiasco::l4_vhw_entry *e = Fiasco::l4_vhw_get_entry_type(vhw, Fiasco::L4_TYPE_VHW_INPUT);
	if (!e) return -2;

	*base = e->mem_start;
	*size = e->mem_size;

	return 0;
}


/**
 * Configure io_mem area containing Fiasco-UX input area
 */
static int map_input_area(addr_t base, size_t size, void **input)
{
	/* request io_mem dataspace */
	Io_mem_connection io_mem(base, size);
	io_mem.on_destruction(Io_mem_connection::KEEP_OPEN);
	Dataspace_capability input_ds_cap = io_mem.dataspace();
	if (!input_ds_cap.valid()) return -2;

	/* attach io_mem dataspace */
	*input = env()->rm_session()->attach(input_ds_cap);
	return 0;
}


int Input_drv::init()
{
	using namespace Genode;

	void *kip = 0;
	try { kip = map_kip(); }
	catch (...) {
		PERR("KIP mapping failed");
		return 1;
	}

	addr_t base; size_t size;
	if (init_input_vhw(kip, &base, &size)) {
		PERR("VHW input init failed");
		return 2;
	}

	PDBG("--- input area is [%lx,%lx) ---", base, base + size);

	void *input = 0;
	if (map_input_area(base, size, &input)) {
		PERR("VHW input area mapping failed");
		return 3;
	}

	input_area = (struct Fiasco::l4input *)input;

	return 0;
}

