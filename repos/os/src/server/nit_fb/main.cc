/*
 * \brief  Framebuffer-to-Nitpicker adapter
 * \author Norman Feske
 * \date   2010-09-09
 */

/*
 * Copyright (C) 2010-2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <nitpicker_session/connection.h>
#include <input/component.h>
#include <os/surface.h>
#include <input/event.h>
#include <os/config.h>
#include <os/static_root.h>
#include <os/server.h>
#include <os/attached_dataspace.h>
#include <timer_session/connection.h>

namespace Nit_fb {

	struct Main;

	using Genode::Static_root;
	using Genode::Signal_rpc_member;

	typedef Genode::Surface_base::Point Point;
	typedef Genode::Surface_base::Area  Area;
	typedef Genode::Surface_base::Rect  Rect;
}


/***************
 ** Utilities **
 ***************/

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


/**
 * Translate input event
 */
static Input::Event translate_event(Input::Event const ev,
                                    Nit_fb::Point const input_origin)
{
	switch (ev.type()) {

	case Input::Event::MOTION:
	case Input::Event::PRESS:
	case Input::Event::RELEASE:
	case Input::Event::FOCUS:
	case Input::Event::LEAVE:
		{
			Nit_fb::Point abs_pos = Nit_fb::Point(ev.ax(), ev.ay()) + input_origin;
			return Input::Event(ev.type(), ev.code(),
			                    abs_pos.x(), abs_pos.y(), 0, 0);
		}

	case Input::Event::INVALID:
	case Input::Event::WHEEL:
		return ev;
	}
	return Input::Event();
}


struct View_updater
{
	virtual void update_view() = 0;
};


/*****************************
 ** Virtualized framebuffer **
 *****************************/

namespace Framebuffer { struct Session_component; }


struct Framebuffer::Session_component : Genode::Rpc_object<Framebuffer::Session>
{
	Nitpicker::Connection &_nitpicker;

	Framebuffer::Session &_nit_fb = *_nitpicker.framebuffer();

	Genode::Signal_context_capability _mode_sigh;

	View_updater &_view_updater;

	/*
	 * Mode as requested by the configuration or by a mode change of our
	 * nitpicker session.
	 */
	Framebuffer::Mode _next_mode = _nitpicker.mode();

	/*
	 * Mode that was returned to the client at the last call of
	 * 'Framebuffer:mode'. The virtual framebuffer must correspond to this
	 * mode. The variable is mutable because it is changed as a side effect of
	 * calling the const 'mode' function.
	 */
	Framebuffer::Mode mutable _active_mode = _next_mode;

	bool _dataspace_is_new = true;


	/**
	 * Constructor
	 */
	Session_component(Nitpicker::Connection &nitpicker,
	                  View_updater &view_updater)
	:
		_nitpicker(nitpicker), _view_updater(view_updater)
	{ }

	void size(Nitpicker::Area size)
	{
		/* ignore calls that don't change the size */
		if (Nitpicker::Area(_next_mode.width(), _next_mode.height()) == size)
			return;

		_next_mode = Framebuffer::Mode(size.w(), size.h(), _next_mode.format());

		if (_mode_sigh.valid())
			Genode::Signal_transmitter(_mode_sigh).submit();
	}

	Nitpicker::Area size() const
	{
		return Nitpicker::Area(_active_mode.width(), _active_mode.height());
	}


	/************************************
	 ** Framebuffer::Session interface **
	 ************************************/

	Genode::Dataspace_capability dataspace() override
	{
		_nitpicker.buffer(_active_mode, false);

		/*
		 * We defer the update of the view until the client calls refresh the
		 * next time. This avoid showing the empty buffer as an intermediate
		 * artifact.
		 */
		_dataspace_is_new = true;

		return _nit_fb.dataspace();
	}

	Mode mode() const override
	{
		_active_mode = _next_mode;
		return _active_mode;
	}

	void mode_sigh(Genode::Signal_context_capability sigh) override
	{
		_mode_sigh = sigh;
	}

	void refresh(int x, int y, int w, int h) override
	{
		if (_dataspace_is_new) {
			_view_updater.update_view();
			_dataspace_is_new = false;
		}

		_nit_fb.refresh(x, y, w, h);
	}

	void sync_sigh(Genode::Signal_context_capability sigh) override
	{
		_nit_fb.sync_sigh(sigh);
	}
};


/******************
 ** Main program **
 ******************/

struct Nit_fb::Main : View_updater
{
	Server::Entrypoint &ep;

	Nitpicker::Connection nitpicker;

	Point position;

	unsigned refresh_rate = 0;

	Nitpicker::Session::View_handle view = nitpicker.create_view();

	Genode::Attached_dataspace input_ds { nitpicker.input()->dataspace() };

	/*
	 * Input and framebuffer sessions provided to our client
	 */
	Input::Session_component       input_session;
	Framebuffer::Session_component fb_session { nitpicker, *this };

	/*
	 * Attach root interfaces to the entry point
	 */
	Static_root<Input::Session>       input_root { ep.manage(input_session) };
	Static_root<Framebuffer::Session> fb_root    { ep.manage(fb_session) };

	/**
	 * View_updater interface
	 */
	void update_view() override
	{
		typedef Nitpicker::Session::Command Command;
		nitpicker.enqueue<Command::Geometry>(view, Rect(position,
		                                                fb_session.size()));
		nitpicker.enqueue<Command::To_front>(view);
		nitpicker.execute();
	}

	void handle_config_update(unsigned)
	{
		Genode::config()->reload();

		Framebuffer::Mode const nit_mode = nitpicker.mode();

		position = Point(config_arg("xpos", 0),
		                 config_arg("ypos", 0));

		refresh_rate = config_arg("refresh_rate", 0);

		fb_session.size(Area(config_arg("width",  nit_mode.width()),
		                     config_arg("height", nit_mode.height())));

		PINF("using xywh=(%d,%d,%d,%d) refresh_rate=%u",
		     position.x(), position.x(),
		     fb_session.size().w(), fb_session.size().h(),
		     refresh_rate);

		update_view();
	}

	Signal_rpc_member<Main> config_update_dispatcher =
		{ ep, *this, &Main::handle_config_update};

	void handle_mode_update(unsigned)
	{
		Framebuffer::Mode const nit_mode = nitpicker.mode();

		fb_session.size(Area(nit_mode.width(), nit_mode.height()));
	}

	Signal_rpc_member<Main> mode_update_dispatcher =
		{ ep, *this, &Main::handle_mode_update};

	void handle_input(unsigned)
	{
		Input::Event const * const events = input_ds.local_addr<Input::Event>();

		unsigned const num = nitpicker.input()->flush();
		for (unsigned i = 0; i < num; i++)
			input_session.submit(translate_event(events[i], position));
	}

	Signal_rpc_member<Main> input_dispatcher =
		{ ep, *this, &Main::handle_input};

	/**
	 * Constructor
	 */
	Main(Server::Entrypoint &ep) : ep(ep)
	{
		input_session.event_queue().enabled(true);

		/*
		 * Announce services
		 */
		Genode::env()->parent()->announce(ep.manage(fb_root));
		Genode::env()->parent()->announce(ep.manage(input_root));

		/*
		 * Apply initial configuration
		 */
		handle_config_update(0);

		/*
		 * Register signal handlers
		 */
		Genode::config()->sigh(config_update_dispatcher);
		nitpicker.mode_sigh(mode_update_dispatcher);
		nitpicker.input()->sigh(input_dispatcher);
	}
};


/************
 ** Server **
 ************/

namespace Server {

	char const *name() { return "nit_fb_ep"; }

	size_t stack_size() { return 4*1024*sizeof(long); }

	void construct(Entrypoint &ep) { static Nit_fb::Main inst(ep); }
}
