/*
 * \brief  Manager of all VM requested console functionality
 * \author Markus Partheymueller
 * \author Norman Feske
 * \author Alexander Boettcher
 * \date   2012-07-31
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 * Copyright (C) 2012 Intel Corporation
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 *
 * The code is partially based on the Vancouver VMM, which is distributed
 * under the terms of the GNU General Public License version 2.
 *
 * Modifications by Intel Corporation are contributed under the terms and
 * conditions of the GNU General Public License version 2.
 */

/* base includes */
#include <util/register.h>

/* nitpicker graphics backend */
#include <nitpicker_gfx/text_painter.h>

#include <nul/motherboard.h>

/* local includes */
#include "console.h"

extern char _binary_mono_tff_start;
Text_painter::Font default_font(&_binary_mono_tff_start);

static struct {
	Genode::uint64_t checksum1 = 0;
	Genode::uint64_t checksum2 = 0;
	unsigned         unchanged = 0;
	bool             cmp_even  = 1;
	bool             active    = true;
	bool             revoked   = false;
} fb_state;


/**
 * Layout of PS/2 mouse packet
 */
struct Ps2_mouse_packet : Genode::Register<32>
{
	struct Packet_size   : Bitfield<0, 3> { };
	struct Left_button   : Bitfield<8, 1> { };
	struct Middle_button : Bitfield<9, 1> { };
	struct Right_button  : Bitfield<10, 1> { };
	struct Rx_high       : Bitfield<12, 1> { };
	struct Ry_high       : Bitfield<13, 1> { };
	struct Rx_low        : Bitfield<16, 8> { };
	struct Ry_low        : Bitfield<24, 8> { };
};


static bool mouse_event(Input::Event const &ev)
{
	using Input::Event;
	if (ev.type() == Event::PRESS || ev.type() == Event::RELEASE) {
		if (ev.code() == Input::BTN_LEFT)   return true;
		if (ev.code() == Input::BTN_MIDDLE) return true;
		if (ev.code() == Input::BTN_RIGHT)  return true;
	}

	if (ev.type() == Event::MOTION)
		return true;

	return false;
}


/**
 * Convert Genode::Input event to PS/2 packet
 *
 * This function updates _left, _middle, and _right as a side effect.
 */
unsigned Seoul::Console::_input_to_ps2mouse(Input::Event const &ev)
{
	/* track state of mouse buttons */
	using Input::Event;
	if (ev.type() == Event::PRESS || ev.type() == Event::RELEASE) {
		bool const pressed = ev.type() == Event::PRESS;
		if (ev.code() == Input::BTN_LEFT)   _left   = pressed;
		if (ev.code() == Input::BTN_MIDDLE) _middle = pressed;
		if (ev.code() == Input::BTN_RIGHT)  _right  = pressed;
	}

	int rx;
	int ry;

	if (ev.absolute_motion()) {
		static Input::Event last_event;
		rx = ev.ax() - last_event.ax();
		ry = ev.ay() - last_event.ay();
		last_event = ev;
	} else {
		rx = ev.rx();
		ry = ev.ry();
	}

	/* clamp relative motion vector to bounds */
	int const boundary = 200;
	rx =  Genode::min(boundary, Genode::max(-boundary, rx));
	ry = -Genode::min(boundary, Genode::max(-boundary, ry));

	/* assemble PS/2 packet */
	Ps2_mouse_packet::access_t packet = 0;
	Ps2_mouse_packet::Packet_size::set  (packet, 3);
	Ps2_mouse_packet::Left_button::set  (packet, _left);
	Ps2_mouse_packet::Middle_button::set(packet, _middle);
	Ps2_mouse_packet::Right_button::set (packet, _right);
	Ps2_mouse_packet::Rx_high::set      (packet, (rx >> 8) & 1);
	Ps2_mouse_packet::Ry_high::set      (packet, (ry >> 8) & 1);
	Ps2_mouse_packet::Rx_low::set       (packet, rx & 0xff);
	Ps2_mouse_packet::Ry_low::set       (packet, ry & 0xff);

	return packet;
}


/* bus callbacks */

bool Seoul::Console::receive(MessageConsole &msg)
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

		enum {
			MEMORY_MODEL_TEXT = 0,
			MEMORY_MODEL_DIRECT_COLOR = 6,
		};

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
			msg.info->memory_model = MEMORY_MODEL_TEXT;
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
			msg.info->memory_model = MEMORY_MODEL_DIRECT_COLOR;
			msg.info->vbe1[0] = 0x5; /* red mask size */
			msg.info->vbe1[1] = 0xb; /* red field position */
			msg.info->vbe1[2] = 0x6; /* green mask size */
			msg.info->vbe1[3] = 0x5; /* green field position */
			msg.info->vbe1[4] = 0x5; /* blue mask size */
			msg.info->vbe1[5] = 0x0; /* blue field position */
			msg.info->vbe1[6] = 0x0; /* reserved mask size */
			msg.info->vbe1[7] = 0x0; /* reserved field position */
			msg.info->colormode = 0x0; /* direct color mode info */
			msg.info->phys_base = 0xe0000000;
			msg.info->_phys_size = _fb_mode.width()*_fb_mode.height()*2;
			return true;
		} else return false;
	}
	return true;
}


bool Seoul::Console::receive(MessageMemRegion &msg)
{
	if (msg.page >= 0xb8 && msg.page <= 0xbf) {

		/* we had a fault in the text framebuffer */
		if (!fb_state.active) fb_state.active = true;
		Logging::printf("Reactivating text buffer loop.\n");

		MessageTimer msg(_timer, _unsynchronized_motherboard.clock()->abstime(1, 1000));
		_unsynchronized_motherboard.bus_timer.send(msg);
	}
	return false;
}


unsigned Seoul::Console::_handle_fb()
{
	if (!_guest_fb || !_regs)
		return 0;

	enum { TEXT_MODE = 0 };

	/* transfer text buffer content into chunky canvas */
	if (_regs->mode == TEXT_MODE) {

		if (fb_state.revoked || !fb_state.active)
			return 0;

		memset(_pixels, 0, _fb_size);

		if (fb_state.cmp_even) fb_state.checksum1 = 0;
		else fb_state.checksum2 = 0;

		for (int j=0; j<25; j++) {
			for (int i=0; i<80; i++) {
				Genode::Surface_base::Point where(i*8, j*15);
				char character = *((char *) (_guest_fb +(_regs->offset << 1) +j*80*2+i*2));
				char colorvalue = *((char *) (_guest_fb+(_regs->offset << 1)+j*80*2+i*2+1));
				char buffer[2]; buffer[0] = character; buffer[1] = 0;
				char fg = colorvalue & 0xf;
				if (fg == 0x8) fg = 0x7;
				unsigned lum = ((fg & 0x8) >> 3)*127;
				Genode::Color color(((fg & 0x4) >> 2)*127+lum, /* R+luminosity */
				                    ((fg & 0x2) >> 1)*127+lum, /* G+luminosity */
				                     (fg & 0x1)*127+lum        /* B+luminosity */);

				Text_painter::paint(*_surface, where, default_font, color, buffer);

				/* Checksum for comparing */
				if (fb_state.cmp_even) fb_state.checksum1 += character;
				else fb_state.checksum2 += character;
			}
		}

		fb_state.cmp_even = !fb_state.cmp_even;

		/* compare checksums to detect changed buffer */
		if (fb_state.checksum1 != fb_state.checksum2) {
			fb_state.unchanged = 0;
			_framebuffer->refresh(0, 0, _fb_mode.width(), _fb_mode.height());
			return 100;
		}

		if (++fb_state.unchanged < 10)
			return fb_state.unchanged * 30;

		/* if we copy the same data 10 times, unmap the text buffer from guest */
		_env.rm().detach((void *)_guest_fb);
		_env.rm().attach_at(_fb_ds, (Genode::addr_t)_guest_fb);

		fb_state.unchanged = 0;
		fb_state.active = false;

		Logging::printf("Deactivated text buffer loop.\n");

		return 0;
	}

	if (!fb_state.revoked) {

		_env.rm().detach((void *)_guest_fb);
		_env.rm().attach_at(_framebuffer->dataspace(),
		                    (Genode::addr_t)_guest_fb);

		/* if the VGA model expects a larger FB, pad to that size. */
		if (_fb_size < _vm_fb_size) {
			Genode::Ram_dataspace_capability _backup =
			  _env.ram().alloc(_vm_fb_size-_fb_size);

			_env.rm().attach_at(_backup,
			                    (Genode::addr_t) (_guest_fb+_fb_size));
		}

		fb_state.revoked = true;
	}
	_framebuffer->refresh(0, 0, _fb_mode.width(), _fb_mode.height());
	return 10;
}


void Seoul::Console::_handle_input()
{
	_input.for_each_event([&] (Input::Event const &ev) {

		if (!fb_state.active) {
			fb_state.active = true;

			MessageTimer msg(_timer, _motherboard()->clock()->abstime(1, 1000));
			_motherboard()->bus_timer.send(msg);
		}

		/* update mouse model (PS2) */
		if (mouse_event(ev)) {
			MessageInput msg(0x10001, _input_to_ps2mouse(ev));
			_motherboard()->bus_input.send(msg);
		}

		if (ev.type() == Input::Event::PRESS)   {
			if (ev.code() <= 0xee) {
				_vkeyb.handle_keycode_press(ev.code());
			}
		}
		if (ev.type() == Input::Event::RELEASE) {
			if (ev.code() <= 0xee) { /* keyboard event */
				_vkeyb.handle_keycode_release(ev.code());
			}
		}
	});
}


void Seoul::Console::register_host_operations(Motherboard &motherboard)
{
	motherboard.bus_console  .add(this, receive_static<MessageConsole>);
	motherboard.bus_memregion.add(this, receive_static<MessageMemRegion>);
	motherboard.bus_timeout  .add(this, receive_static<MessageTimeout>);

	MessageTimer msg;
	if (!motherboard.bus_timer.send(msg))
		Logging::panic("%s can't get a timer", __PRETTY_FUNCTION__);

	_timer = msg.nr;
}


bool Seoul::Console::receive(MessageTimeout &msg) {
	if (msg.nr != _timer)
		return false;

	unsigned next_timeout_ms = _handle_fb();

	if (next_timeout_ms) {
		MessageTimer msg_t(_timer, _unsynchronized_motherboard.clock()->abstime(next_timeout_ms, 1000));
		_unsynchronized_motherboard.bus_timer.send(msg_t);
	}

	return true;
}


Seoul::Console::Console(Genode::Env &env, Synced_motherboard &mb,
                        Motherboard &unsynchronized_motherboard,
                        Genode::size_t vm_fb_size,
                        Genode::Dataspace_capability fb_ds)
:
	_env(env),
	_unsynchronized_motherboard(unsynchronized_motherboard),
	_motherboard(mb), _fb_ds(fb_ds), _vm_fb_size(vm_fb_size)
{
	_input.sigh(_signal_input);

	try {
		using Framebuffer::Mode;
		_framebuffer.construct(_env, Mode(0, 0, Mode::INVALID));
	} catch (...) {
		Genode::error("Headless mode - no framebuffer session available");
		return;
	}

	_fb_size = Genode::Dataspace_client(_framebuffer->dataspace()).size();
	_fb_mode = _framebuffer->mode();
	_pixels  = _env.rm().attach(_framebuffer->dataspace());

	_surface.construct(reinterpret_cast<Genode::Pixel_rgb565 *>(_pixels),
	                   Genode::Surface_base::Area(_fb_mode.width(),
	                   _fb_mode.height()));
}
