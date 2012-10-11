/*
 * \brief  Manager of all VM requested console functionality
 * \author Markus Partheymueller
 * \date   2012-07-31
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 * Copyright (C) 2012 Intel Corporation
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 *
 * The code is partially based on the Vancouver VMM, which is distributed
 * under the terms of the GNU General Public License version 2.
 *
 * Modifications by Intel Corporation are contributed under the terms and
 * conditions of the GNU General Public License version 2.
 */

/* local includes */
#include <console.h>
#include <keyboard.h>

/* Genode includes */
#include <base/snprintf.h>

/* nitpicker graphics backend */
#include <nitpicker_gfx/chunky_canvas.h>
#include <nitpicker_gfx/pixel_rgb565.h>
#include <nitpicker_gfx/font.h>

extern char _binary_mono_tff_start;
Font default_font(&_binary_mono_tff_start);

extern Genode::Lock global_lock;
extern bool console_init;

using Genode::env;
using Genode::Dataspace_client;

bool fb_active = true;


static unsigned mouse_value(Input::Event * ev)
{
	/* bit 3 is always set */
	unsigned ret = 0x8;

	/* signs and movements */
	int x=0, y=0;
	if (ev->rx() > 0) x = 1;
	if (ev->rx() < 0) x = -1;
	if (ev->ry() > 0) y = 1;
	if (ev->ry() < 0) y = -1;

	if (x > 0)
		ret |= (1 << 8);
	if (x < 0)
		ret |= (0xfe << 8) | (1 << 4);
	if (y < 0) /* nitpicker's negative is PS2 positive */
		ret |= (1 << 16);
	if (y > 0)
		ret |= (0xfe << 16) | (1 << 5);

	/* buttons */
	ret |= ((ev->code() == Input::BTN_MIDDLE ? 1 : 0) << 2);
	ret |= ((ev->code() == Input::BTN_RIGHT ? 1 : 0) << 1);
	ret |= ((ev->code() == Input::BTN_LEFT ? 1 : 0) << 0);

	/* ps2mouse model expects 3 in the first byte */
	return (ret << 8) | 0x3;
}

/*
 * Console implementation
 */

/* bus callbacks */

bool Vancouver_console::receive(MessageConsole &msg)
{
	if (msg.type == MessageConsole::TYPE_ALLOC_VIEW) {
		_guest_fb = msg.ptr;

		if (msg.size < _fb_size)
			_fb_size = msg.size;

		_regs = msg.regs;

		msg.view = 0;
	} else if (msg.type == MessageConsole::TYPE_SWITCH_VIEW) {
		/* XXX: For now, we only have one view. */
	} else if (msg.type == MessageConsole::TYPE_GET_MODEINFO) {

		/*
		 * We supply two modes to the guest, text mode and one
		 * configured graphics mode 16-bit.
		 */
		if (msg.index == 0) {
			msg.info->_vesa_mode = 3;
			msg.info->attr = 0x1;
			msg.info->resolution[0] = 80;
			msg.info->resolution[1] = 25;
			msg.info->bytes_per_scanline = 80*2;
			msg.info->bytes_scanline = 80*2;
			msg.info->bpp = 4;
			msg.info->phys_base = 0xb8000;
			msg.info->_phys_size = 0x8000;
			return true;

		} else if (msg.index == 1) {

			/*
			 * It's important to set the _vesa_mode field, otherwise the
			 * device model is going to ignore this mode.
			 */
			msg.info->_vesa_mode = 0x114;
			msg.info->attr = 0x39f;
			msg.info->resolution[0] = _fb_mode.width();
			msg.info->resolution[1] = _fb_mode.height();
			msg.info->bytes_per_scanline = _fb_mode.width()*2;
			msg.info->bytes_scanline = _fb_mode.width()*2;
			msg.info->bpp = 16;
			msg.info->vbe1[0] = 0x5; /* red mask size */
			msg.info->vbe1[1] = 0xb; /* red field position */
			msg.info->vbe1[2] = 0x6; /* green mask size */
			msg.info->vbe1[3] = 0x5; /* green field position */
			msg.info->vbe1[4] = 0x5; /* blue mask size */
			msg.info->vbe1[5] = 0x0; /* blue field position */
			msg.info->vbe1[6] = 0x0; /* reserved mask size */
			msg.info->vbe1[7] = 0x0; /* reserved field position */
			msg.info->vbe1[8] = 0x0; /* direct color mode info */
			msg.info->phys_base = 0xe0000000;
			msg.info->_phys_size = _fb_mode.width()*_fb_mode.height()*2;
			return true;
		} else return false;
	}
	return true;
}


bool Vancouver_console::receive(MessageMemRegion &msg)
{
	if (msg.page >= 0xb8 && msg.page <= 0xbf) {

		/* we had a fault in the text framebuffer */
		if (!fb_active) fb_active = true;
		Logging::printf("Reactivating text buffer loop.\n");
	}
}


void Vancouver_console::entry()
{
	Logging::printf("Hello, this is VancouverConsole.\n");

	/* register host operations */
	_mb.bus_console.add(this, receive_static<MessageConsole>);
	_mb.bus_memregion.add(this, receive_static<MessageMemRegion>);

	/* create environment for input/output */

	/*
	 * Init sessions to the required external services
	 */
	enum { CONFIG_ALPHA = false };
	static Framebuffer::Connection framebuffer;
	static Input::Connection       input;
	static Timer::Connection       timer;

	Genode::Dataspace_capability ev_ds_cap = input.dataspace();

	Input::Event *ev_buf = static_cast<Input::Event *>
	                       (env()->rm_session()->attach(ev_ds_cap));

	_fb_size = Dataspace_client(framebuffer.dataspace()).size();
	_fb_mode = framebuffer.mode();

	_pixels = env()->rm_session()->attach(framebuffer.dataspace());

	Chunky_canvas<Pixel_rgb565> canvas((Pixel_rgb565 *) _pixels,
	                                   Area(_fb_mode.width(),
	                                   _fb_mode.height()));

	/*
	 * Handle input events
	 */
	unsigned long count = 0;
	bool revoked = false;
	Vancouver_keyboard vkeyb(_mb);

	Genode::uint64_t checksum1 = 0;
	Genode::uint64_t checksum2 = 0;
	unsigned unchanged = 0;
	bool cmp_even = 1;

	console_init = true;
	while (1) {
		while (!input.is_pending()) {

			/* transfer text buffer content into chunky canvas */
			if (_regs && ++count % 10 == 0 && _regs->mode == 0
			 && _guest_fb && !revoked && fb_active) {

				memset(_pixels, 0, _fb_size);
				if (cmp_even) checksum1 = 0;
				else checksum2 = 0;
				for (int j=0; j<25; j++) {
					for (int i=0; i<80; i++) {
						Point where(i*8, j*15);
						char character = *((char *) (_guest_fb+0x18000+j*80*2+i*2));
						char colorvalue = *((char *) (_guest_fb+0x18000+j*80*2+i*2+1));
						char buffer[2];
						Genode::snprintf(buffer, 1, "%c", character);
						char fg = colorvalue & 0xf;
						if (fg == 0x8) fg = 0x7;
						unsigned lum = ((fg & 0x8) >> 3)*127;
						Color color(((fg & 0x4) >> 2)*127+lum, /* R+luminosity */
						            ((fg & 0x2) >> 1)*127+lum, /* G+luminosity */
						            (fg & 0x1)*127+lum         /* B+luminosity */);

						canvas.draw_string(where, &default_font, color, buffer);

						/* Checksum for comparing */
						if (cmp_even) checksum1 += character;
						else checksum2 += character;
					}
				}
				/* compare checksums to detect idle buffer */
				if (checksum1 == checksum2) {
					unchanged++;

					/* if we copy the same data 10 times, unmap the text buffer from guest */
					if (unchanged == 10) {

						/* protect against thread interference */
						global_lock.lock();

						env()->rm_session()->detach((void *)_guest_fb);
						env()->rm_session()->attach_at(_fb_ds, (Genode::addr_t)_guest_fb);
						unchanged = 0;
						fb_active = false;

						global_lock.unlock();
						Logging::printf("Deactivated text buffer loop.\n");
					}
				} else unchanged = 0;
				cmp_even = !cmp_even;

			} else if (_regs && _guest_fb && _regs->mode != 0) {

				if (!revoked) {

					/* protect against thread interference */
					global_lock.lock();

					env()->rm_session()->detach((void *)_guest_fb);
					env()->rm_session()->attach_at(framebuffer.dataspace(),
					                               (Genode::addr_t)_guest_fb);

					/* if the VGA model expects a larger FB, pad to that size. */
					if (_fb_size < _vm_fb_size) {
						Genode::Ram_dataspace_capability _backup =
						  Genode::env()->ram_session()->alloc(_vm_fb_size-_fb_size);

						env()->rm_session()->attach_at(_backup,
						                               (Genode::addr_t) (_guest_fb+_fb_size));
					}

					revoked = true;

					global_lock.unlock();
				}
			}
			framebuffer.refresh(0, 0, _fb_mode.width(), _fb_mode.height());

			timer.msleep(10);
		}

		for (int i = 0, num_ev = input.flush(); i < num_ev; i++) {
			Input::Event *ev = &ev_buf[i];

			/* update mouse model (PS2) */
			unsigned mouse = mouse_value(ev);
			MessageInput msg(0x10001, mouse);
			_mb.bus_input.send(msg);

			if (ev->type() == Input::Event::PRESS)   {
				if (ev->code() <= 0xee) {
					vkeyb.handle_keycode_press(ev->code());
				}
			}
			if (ev->type() == Input::Event::RELEASE) {
				if (ev->code() <= 0xee) { /* keyboard event */
					vkeyb.handle_keycode_release(ev->code());
				}
			}
		}
	}
}


Vancouver_console::Vancouver_console(Motherboard &mb, Genode::size_t vm_fb_size,
                                     Genode::Dataspace_capability fb_ds)
:
	_vm_fb_size(vm_fb_size), _mb(mb), _fb_size(0), _pixels(0), _guest_fb(0),
	_regs(0), _fb_ds(fb_ds)
{
	start();
}
