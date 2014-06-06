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
#include <cap_session/connection.h>
#include <nitpicker_session/connection.h>
#include <dataspace/client.h>
#include <input_session/input_session.h>
#include <input/event.h>
#include <os/config.h>
#include <os/static_root.h>
#include <timer_session/connection.h>


namespace Input {

	/**
	 * Input session applying a position offset to absolute motion events
	 */
	class Session_component : public Genode::Rpc_object<Session>
	{
		private:

			/**
			 * Offset to be applied to absolute motion events
			 */
			int _dx, _dy;

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
				_dx(dx), _dy(dy),
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

			Genode::Dataspace_capability dataspace() override { return _to_input_ds; }

			bool is_pending() const override { return _from_input->is_pending(); }

			int flush() override
			{
				/* flush events at input session */
				int num_events = _from_input->flush();

				/* copy events from input buffer to client buffer */
				for (int i = 0; i < num_events; i++) {
					Input::Event e = _from_ev_buf[i];

					/* apply view offset to absolute motion events */
					if (e.is_absolute_motion())
						e = Event(e.type(), e.code(),
						          e.ax() + _dx, e.ay() + _dy, 0, 0);
					_to_ev_buf[i] = e;
				}
				return num_events;
			}

			void sigh(Genode::Signal_context_capability sigh) override
			{
				_from_input->sigh(sigh);
			}
	};
}


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
	 * Read arguments from config
	 */
	Config_args cfg;
	long view_x = cfg.xpos,  view_y = cfg.ypos,
	     view_w = cfg.width, view_h = cfg.height;

	/*
	 * Open Nitpicker session
	 */
	static Nitpicker::Connection nitpicker;

	/*
	 * If no config was provided, use screen size of Nitpicker
	 */
	if (view_w == 0 || view_h == 0) {
		Framebuffer::Mode const mode = nitpicker.mode();
		view_w = mode.width(), view_h = mode.height();
	}

	/*
	 * Setup virtual framebuffer
	 */
	Framebuffer::Mode const mode(view_w, view_h, Framebuffer::Mode::RGB565);
	nitpicker.buffer(mode, false);

	PINF("using xywh=(%ld,%ld,%ld,%ld) refresh_rate=%u",
	     view_x, view_y, view_w, view_h, cfg.refresh_rate);

	/*
	 * Create Nitpicker view and bring it to front
	 */
	Nitpicker::Session::View_handle view = nitpicker.create_view();
	typedef Nitpicker::Session::Command Command;
	Nitpicker::Rect geometry(Nitpicker::Point(view_x, view_y),
	                         Nitpicker::Area(view_w, view_h));
	nitpicker.enqueue<Command::Geometry>(view, geometry);
	nitpicker.enqueue<Command::To_front>(view);
	nitpicker.execute();

	/*
	 * Initialize server entry point
	 */
	enum { STACK_SIZE = 4096 };
	static Cap_connection cap;
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "nitfb_ep");

	/*
	 * Let the entry point serve the framebuffer and input root interfaces
	 */
	static Static_root<Framebuffer::Session> fb_root(nitpicker.framebuffer_session());

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
	Signal_receiver sig_rec;
	Signal_context sig_ctx;
	config()->sigh(sig_rec.manage(&sig_ctx));

	for (;;) {

		bool reload_config = false;

		if (!cfg.refresh_rate) {

			sig_rec.wait_for_signal();
			reload_config = true;

		} else {

			static Timer::Connection timer;
			static Framebuffer::Session_client nit_fb(nitpicker.framebuffer_session());

			timer.msleep(cfg.refresh_rate);
			nit_fb.refresh(0, 0, view_w, view_h);

			if (sig_rec.pending()) {
				sig_rec.wait_for_signal();
				reload_config = true;
			}
		}

		if (reload_config) {
			try {
				config()->reload();
				cfg = Config_args();

				Nitpicker::Rect geometry(Nitpicker::Point(cfg.xpos, cfg.ypos),
				                         Nitpicker::Area(cfg.width, cfg.height));
				nitpicker.enqueue<Command::Geometry>(view, geometry);
				nitpicker.execute();
			} catch (...) {
				PERR("Error while reloading config");
			}
		}
	}
	return 0;
}

