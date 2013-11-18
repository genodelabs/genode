/*
 * \brief  Framebuffer-to-Nitpicker adapter
 * \author Norman Feske
 * \date   2010-09-09
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/sleep.h>
#include <base/rpc_server.h>
#include <nitpicker_view/client.h>
#include <cap_session/connection.h>
#include <nitpicker_session/connection.h>
#include <dataspace/client.h>
#include <input_session/input_session.h>
#include <input/event.h>
#include <root/root.h>
#include <os/config.h>
#include <os/static_root.h>
#include <timer_session/connection.h>
#include <framebuffer_session/framebuffer_session.h>


namespace Input       { class Session_component; }
namespace Framebuffer { class Session_component; }


/**
 * Input session applying a position offset to absolute motion events
 */
class Input::Session_component : public Genode::Rpc_object<Session>
{
	private:

		/**
		 * Offset to be applied to absolute motion events
		 */
		int _dx, _dy;

		bool _focused;

		/**
		 * Input session, from which we fetch events
		 */
		Input::Session              *_from_input;
		Genode::Dataspace_capability _from_input_ds;
		Genode::size_t               _from_ev_buf_size;
		Input::Event                *_from_ev_buf;

		/**
		 * Input session, to which to provide events
		 */
		Genode::Dataspace_capability _to_input_ds;
		Input::Event                *_to_ev_buf;

		/**
		 * Shortcut for mapping an event buffer locally
		 */
		Input::Event *_map_ev_buf(Genode::Dataspace_capability ds_cap) {
			return Genode::env()->rm_session()->attach(ds_cap); }

	public:

		/**
		 * Constructor
		 *
		 * \param dx, dy      offset to be added to absolute motion events
		 * \param from_input  input session from where to get input events
		 */
		Session_component(int dx, int dy, Input::Session *from_input)
		:
			_dx(dx), _dy(dy), _focused(false),
			_from_input(from_input),
			_from_input_ds(from_input->dataspace()),
			_from_ev_buf_size(Genode::Dataspace_client(_from_input_ds).size()),
			_from_ev_buf(_map_ev_buf(_from_input_ds)),
			_to_input_ds(Genode::env()->ram_session()->alloc(_from_ev_buf_size)),
			_to_ev_buf(_map_ev_buf(_to_input_ds))
		{ }


		/*****************************
		 ** Input session interface **
		 *****************************/

		Genode::Dataspace_capability dataspace() { return _to_input_ds; }

		bool is_pending() const { return _from_input->is_pending(); }

		bool is_focused() const { return _focused; }

		int flush()
		{
			/* flush events at input session */
			int num_events = _from_input->flush();

			/* copy events from input buffer to client buffer */
			for (int i = 0; i < num_events; i++) {
				Input::Event e = _from_ev_buf[i];

				/* track focus state */
				if (e.type() == Input::Event::FOCUS) {
					_focused = e.code();
				}

				/* apply view offset to absolute motion events */
				if (e.is_absolute_motion())
					e = Event(e.type(), e.code(),
					          e.ax() + _dx, e.ay() + _dy, 0, 0);
				_to_ev_buf[i] = e;
			}
			return num_events;
		}
};


class Framebuffer::Session_component : public Genode::Rpc_object<Session>
{
	private:

		Session *_wrapped_framebuffer;
		Genode::Dataspace_capability _ds_cap;
		unsigned _refresh_cnt;

	public:

		Session_component(Session *wrapped_framebuffer,
		                  Genode::Dataspace_capability fb_cap)
		: _wrapped_framebuffer(wrapped_framebuffer),
		  _ds_cap(fb_cap), _refresh_cnt(0) { }

		unsigned refresh_cnt() const { return _refresh_cnt; }


		/***********************************
		 ** Framebuffer session interface **
		 ***********************************/

		Genode::Dataspace_capability dataspace() {
			return _ds_cap; }

		void release() { }

		Mode mode() const
		{
			return _wrapped_framebuffer->mode();
		}

		void mode_sigh(Genode::Signal_context_capability) { }

		void refresh(int x, int y, int w, int h)
		{
			_refresh_cnt++;
			_wrapped_framebuffer->refresh(x, y, w, h);
		}
};


enum { DITHER_SIZE = 16, DITHER_MASK = DITHER_SIZE - 1 };

static const int dither_matrix[DITHER_SIZE][DITHER_SIZE] = {
  {   0,192, 48,240, 12,204, 60,252,  3,195, 51,243, 15,207, 63,255 },
  { 128, 64,176,112,140, 76,188,124,131, 67,179,115,143, 79,191,127 },
  {  32,224, 16,208, 44,236, 28,220, 35,227, 19,211, 47,239, 31,223 },
  { 160, 96,144, 80,172,108,156, 92,163, 99,147, 83,175,111,159, 95 },
  {   8,200, 56,248,  4,196, 52,244, 11,203, 59,251,  7,199, 55,247 },
  { 136, 72,184,120,132, 68,180,116,139, 75,187,123,135, 71,183,119 },
  {  40,232, 24,216, 36,228, 20,212, 43,235, 27,219, 39,231, 23,215 },
  { 168,104,152, 88,164,100,148, 84,171,107,155, 91,167,103,151, 87 },
  {   2,194, 50,242, 14,206, 62,254,  1,193, 49,241, 13,205, 61,253 },
  { 130, 66,178,114,142, 78,190,126,129, 65,177,113,141, 77,189,125 },
  {  34,226, 18,210, 46,238, 30,222, 33,225, 17,209, 45,237, 29,221 },
  { 162, 98,146, 82,174,110,158, 94,161, 97,145, 81,173,109,157, 93 },
  {  10,202, 58,250,  6,198, 54,246,  9,201, 57,249,  5,197, 53,245 },
  { 138, 74,186,122,134, 70,182,118,137, 73,185,121,133, 69,181,117 },
  {  42,234, 26,218, 38,230, 22,214, 41,233, 25,217, 37,229, 21,213 },
  { 170,106,154, 90,166,102,150, 86,169,105,153, 89,165,101,149, 85 }
};


class Alpha_channel
{
	private:

		unsigned char *_alpha_base;
		unsigned       _w, _h;

	public:

		enum { MIN = 250 + 80, MAX = 255 + (1 << 7) + 256 };

		Alpha_channel(unsigned char *alpha_base, unsigned w, unsigned h)
		: _alpha_base(alpha_base), _w(w), _h(h) { }

		void set(int alpha)
		{
			if (!_alpha_base) return;

			unsigned char *dst = _alpha_base;
			for (unsigned y = 0; y < _h; y++) {
				const int *dither_row = dither_matrix[y & DITHER_MASK];
				int row_alpha = alpha;
				row_alpha -= (y*256)/_h;
				for (unsigned x = 0; x < _w; x++) {
					int a = row_alpha - (dither_row[x & DITHER_MASK] >> 1);

					/* clamp value to valid 8-bit alpha range */
					*dst++ = Genode::max(0, Genode::min(255, a));
				}
			}
		}
};


/**
 * Read integer value from config attribute
 */
long config_arg(const char *attr, long default_value)
{
	long res = default_value;
	try { Genode::config()->xml_node().attribute(attr).value(&res); }
	catch (...) { }
	return res;
}


struct Config_args
{
	long xpos, ypos, width, height;
	unsigned refresh_rate;

	Config_args()
	:
		xpos(config_arg("xpos",   0)),
		ypos(config_arg("ypos",   0)),
		width(config_arg("width",  0)),
		height(config_arg("height", 0)),
		refresh_rate(config_arg("refresh_rate", 0))
	{ }
};


int main(int argc, char **argv)
{
	using namespace Genode;

	/*
	 * Open Nitpicker session
	 */
	static Nitpicker::Connection nitpicker;
	static Framebuffer::Mode const scr_mode = nitpicker.mode();

	/*
	 * Read arguments from config. If no config was provided, use screen size
	 * of Nitpicker as size of virtual framebuffer.
	 */
	Config_args cfg;
	bool const use_defaults = (cfg.width == 0) || (cfg.height == 0);
	long const view_x = cfg.xpos,  view_y = cfg.ypos,
	           view_w = use_defaults ? scr_mode.width()  : cfg.width,
	           view_h = use_defaults ? scr_mode.height() : cfg.height;

	/*
	 * Set up virtual framebuffer
	 */
	static Framebuffer::Mode const mode(view_w, view_h, scr_mode.format());
	nitpicker.buffer(mode, true);
	Framebuffer::Session_client nit_fb(nitpicker.framebuffer_session());

	/*
	 * Initialize alpha channel and input mask
	 */
	unsigned char *alpha_base = 0;
	Genode::Dataspace_capability fb_ds = nitpicker.framebuffer()->dataspace();
	void *fb_base = Genode::env()->rm_session()->attach(fb_ds);
	if (mode.format() == Framebuffer::Mode::RGB565) {
		size_t num_pixels = view_w*view_h;
		alpha_base = (unsigned char *)fb_base + 2*num_pixels;
		unsigned char *input_mask_base = alpha_base + num_pixels;
		Genode::memset(input_mask_base, 255, num_pixels);
	}

	static Alpha_channel alpha_channel(alpha_base, view_w, view_h);
	int alpha = Alpha_channel::MIN;
	alpha_channel.set(alpha);

	PINF("using xywh=(%ld,%ld,%ld,%ld)", view_x, view_y, view_w, view_h);

	/*
	 * Create Nitpicker view and bring it to front
	 */
	Nitpicker::View_client view(nitpicker.create_view());
	view.viewport(view_x, view_y, view_w, view_h, 0, 0, false);
	view.stack(Nitpicker::View_capability(), true, true);

	/*
	 * Initialize server entry point
	 */
	enum { STACK_SIZE = 4096 };
	static Cap_connection cap;
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "fade_fb_ep");

	/*
	 * Initialize monitor of the framebuffer session interface
	 */
	static Framebuffer::Session_component fb_monitor(nitpicker.framebuffer(), fb_ds);

	/*
	 * Let the entry point serve the framebuffer and input root interfaces
	 */
	static Static_root<Framebuffer::Session> fb_root(ep.manage(&fb_monitor));

	/*
	 * Pre-initialize single client input session
	 */
	static Input::Session_client    nit_input(nitpicker.input_session());
	static Input::Session_component input_session(-view_x, -view_y, &nit_input);

	/*
	 * Attach input root interface to the entry point
	 */
	static Static_root<Input::Session> input_root(ep.manage(&input_session));

	/*
	 * Announce services
	 */
	env()->parent()->announce(ep.manage(&fb_root));
	env()->parent()->announce(ep.manage(&input_root));

	/*
	 * Register signal handler for config changes
	 */
	static Signal_receiver sig_rec;
	static Signal_context  sig_ctx;
	config()->sigh(sig_rec.manage(&sig_ctx));

	/*
	 * The following enum values are in milliseconds
	 */
	enum { FADE_OUT_SPEED        = 15,
	       FADE_IN_SPEED         = 30,
	       FOCUS_SAMPLE_RATE     = 100,
	       FADE_OUT_REFRESH_RATE = 40 };

	enum Fade_state { FOCUSED, FADE_OUT, UNFOCUSED, FADE_IN } fade_state = UNFOCUSED;

	while (true) {

		unsigned sleep_period = FOCUS_SAMPLE_RATE;
		if (cfg.refresh_rate)
			sleep_period = min(cfg.refresh_rate, sleep_period);
		if (fade_state == FADE_OUT || fade_state == FADE_IN)
			sleep_period = min(sleep_period, (unsigned)FADE_OUT_REFRESH_RATE);

		static Timer::Connection timer;
		timer.msleep(sleep_period);

		/*
		 * State machine for deriving the fade state from the keyboard focus
		 */
		bool const is_focused = input_session.is_focused();
		switch (fade_state) {
		case FOCUSED:

			if (!is_focused)
				fade_state = FADE_OUT;
			break;

		case FADE_OUT:

			if (alpha <= Alpha_channel::MIN)
				fade_state = UNFOCUSED;

			if (is_focused)
				fade_state = FADE_IN;
			break;

		case UNFOCUSED:

			if (is_focused)
				fade_state = FADE_IN;
			break;

		case FADE_IN:

			if (alpha >= Alpha_channel::MAX)
				fade_state = FOCUSED;

			if (!is_focused)
				fade_state = FADE_OUT;
			break;
		}

		bool do_refresh = (cfg.refresh_rate != 0) ? true : false;

		/* reload config if needed */
		if (sig_rec.pending()) {
			sig_rec.wait_for_signal();

			try {
				config()->reload();
				cfg = Config_args();
				view.viewport(cfg.xpos, cfg.ypos, cfg.width, cfg.height, 0, 0, true);
				do_refresh = true;
			} catch (...) {
				PERR("Error while reloading config");
			}
		}

		/* fade out */
		if (fade_state == FADE_OUT) {
			alpha = max((int)Alpha_channel::MIN, alpha - FADE_OUT_SPEED);
			alpha_channel.set(alpha);
			do_refresh = true;
		}

		/* fade in */
		if (fade_state == FADE_IN) {
			alpha = min((int)Alpha_channel::MAX, alpha + FADE_IN_SPEED);
			alpha_channel.set(alpha);
			do_refresh = true;
		}

		if (do_refresh)
			nitpicker.framebuffer()->refresh(0, 0, view_w, view_h);
	}
	return 0;
}

