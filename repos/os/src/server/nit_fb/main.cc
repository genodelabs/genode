/*
 * \brief  Framebuffer-to-Nitpicker adapter
 * \author Norman Feske
 * \date   2010-09-09
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <nitpicker_session/connection.h>
#include <input/component.h>
#include <os/surface.h>
#include <input/event.h>
#include <os/static_root.h>
#include <base/attached_rom_dataspace.h>
#include <base/component.h>

namespace Nit_fb {

	struct Main;

	using Genode::Static_root;
	using Genode::Signal_handler;
	using Genode::Xml_node;
	using Genode::size_t;

	typedef Genode::Surface_base::Point Point;
	typedef Genode::Surface_base::Area  Area;
	typedef Genode::Surface_base::Rect  Rect;
}


/***************
 ** Utilities **
 ***************/

/**
 * Translate input event
 */
static Input::Event translate_event(Input::Event        ev,
                                    Nit_fb::Point const input_origin,
                                    Nit_fb::Area  const boundary)
{
	using Nit_fb::Point;

	/* function to clamp point to bounday */
	auto clamp = [boundary] (Point p) {
		return Point(Genode::min((int)boundary.w() - 1, Genode::max(0, p.x())),
		             Genode::min((int)boundary.h() - 1, Genode::max(0, p.y()))); };

	/* function to translate point to 'input_origin' */
	auto translate = [input_origin] (Point p) { return p - input_origin; };

	ev.handle_absolute_motion([&] (int x, int y) {
		Point p = clamp(translate(Point(x, y)));
		ev = Input::Absolute_motion{p.x(), p.y()};
	});

	ev.handle_touch([&] (Input::Touch_id id, float x, float y) {
		Point p = clamp(translate(Point(x, y)));
		ev = Input::Touch{id, (float)p.x(), (float)p.y()};
	});

	return ev;
}


struct View_updater : Genode::Interface
{
	virtual void update_view() = 0;
};


/*****************************
 ** Virtualized framebuffer **
 *****************************/

namespace Framebuffer { struct Session_component; }


struct Framebuffer::Session_component : Genode::Rpc_object<Framebuffer::Session>
{
	Genode::Pd_session const &_pd;

	Nitpicker::Connection &_nitpicker;

	Framebuffer::Session &_nit_fb = *_nitpicker.framebuffer();

	Genode::Signal_context_capability _mode_sigh { };

	Genode::Signal_context_capability _sync_sigh { };

	View_updater &_view_updater;

	Framebuffer::Mode::Format _format = _nitpicker.mode().format();

	/*
	 * Mode as requested by the configuration or by a mode change of our
	 * nitpicker session.
	 */
	Framebuffer::Mode _next_mode;

	typedef Genode::size_t size_t;

	/*
	 * Number of bytes used for backing the current virtual framebuffer at
	 * nitpicker.
	 */
	size_t _buffer_num_bytes = 0;

	/*
	 * Mode that was returned to the client at the last call of
	 * 'Framebuffer:mode'. The virtual framebuffer must correspond to this
	 * mode. The variable is mutable because it is changed as a side effect of
	 * calling the const 'mode' function.
	 */
	Framebuffer::Mode mutable _active_mode = _next_mode;

	bool _dataspace_is_new = true;

	bool _ram_suffices_for_mode(Framebuffer::Mode mode) const
	{
		/* calculation in bytes */
		size_t const used      = _buffer_num_bytes,
		             needed    = Nitpicker::Session::ram_quota(mode, false),
		             usable    = _pd.avail_ram().value,
		             preserved = 64*1024;

		return used + usable > needed + preserved;
	}


	/**
	 * Constructor
	 */
	Session_component(Genode::Pd_session const &pd,
	                  Nitpicker::Connection &nitpicker,
	                  View_updater &view_updater,
	                  Framebuffer::Mode initial_mode)
	:
		_pd(pd), _nitpicker(nitpicker), _view_updater(view_updater),
		_next_mode(initial_mode)
	{ }

	void size(Nitpicker::Area size)
	{
		/* ignore calls that don't change the size */
		if (Nitpicker::Area(_next_mode.width(), _next_mode.height()) == size)
			return;

		Framebuffer::Mode const mode(size.w(), size.h(), _next_mode.format());

		if (!_ram_suffices_for_mode(mode)) {
			Genode::warning("insufficient RAM for mode ", mode);
			return;
		}

		_next_mode = mode;

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

		_buffer_num_bytes =
			Genode::max(_buffer_num_bytes,
			            Nitpicker::Session::ram_quota(_active_mode, false));

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
		/*
		 * Keep a component-local copy of the signal capability. Otherwise,
		 * NOVA would revoke the capability from further recipients (in this
		 * case the nitpicker instance we are using) once we revoke the
		 * capability locally.
		 */
		_sync_sigh = sigh;

		_nit_fb.sync_sigh(sigh);
	}
};


/******************
 ** Main program **
 ******************/

struct Nit_fb::Main : View_updater
{
	Genode::Env &env;

	Genode::Attached_rom_dataspace config_rom { env, "config" };

	Nitpicker::Connection nitpicker { env };

	Point position { 0, 0 };

	unsigned refresh_rate = 0;

	typedef Nitpicker::Session::View_handle View_handle;

	View_handle view = nitpicker.create_view();

	Genode::Attached_dataspace input_ds { env.rm(), nitpicker.input()->dataspace() };

	struct Initial_size
	{
		long const _width  { 0 };
		long const _height { 0 };

		bool set { false };

		Initial_size(Genode::Xml_node config)
		:
			_width (config.attribute_value("initial_width",  0L)),
			_height(config.attribute_value("initial_height", 0L))
		{ }

		unsigned width(Framebuffer::Mode const &mode) const
		{
			if (_width > 0) return _width;
			if (_width < 0) return mode.width() + _width;
			return mode.width();
		}

		unsigned height(Framebuffer::Mode const &mode) const
		{
			if (_height > 0) return _height;
			if (_height < 0) return mode.height() + _height;
			return mode.height();
		}

		bool valid() const { return _width != 0 && _height != 0; }

	} _initial_size { config_rom.xml() };

	Framebuffer::Mode _initial_mode()
	{
		return Framebuffer::Mode(_initial_size.width (nitpicker.mode()),
		                         _initial_size.height(nitpicker.mode()),
		                         nitpicker.mode().format());
	}

	/*
	 * Input and framebuffer sessions provided to our client
	 */
	Input::Session_component       input_session { env, env.ram() };
	Framebuffer::Session_component fb_session { env.pd(), nitpicker, *this, _initial_mode() };

	/*
	 * Attach root interfaces to the entry point
	 */
	Static_root<Input::Session>       input_root { env.ep().manage(input_session) };
	Static_root<Framebuffer::Session> fb_root    { env.ep().manage(fb_session) };

	/**
	 * View_updater interface
	 */
	void update_view() override
	{
		typedef Nitpicker::Session::Command Command;
		nitpicker.enqueue<Command::Geometry>(view, Rect(position,
		                                                fb_session.size()));
		nitpicker.enqueue<Command::To_front>(view, View_handle());
		nitpicker.execute();
	}

	/**
	 * Return screen-coordinate origin, depening on the config and screen mode
	 */
	static Point _coordinate_origin(Framebuffer::Mode mode, Xml_node config)
	{
		char const * const attr = "origin";

		if (!config.has_attribute(attr))
			return Point(0, 0);

		typedef Genode::String<32> Value;
		Value const value = config.attribute_value(attr, Value());

		if (value == "top_left")     return Point(0, 0);
		if (value == "top_right")    return Point(mode.width(), 0);
		if (value == "bottom_left")  return Point(0, mode.height());
		if (value == "bottom_right") return Point(mode.width(), mode.height());

		warning("unsupported ", attr, " attribute value '", value, "'");
		return Point(0, 0);
	}

	void _update_size()
	{
		Xml_node const config = config_rom.xml();

		Framebuffer::Mode const nit_mode = nitpicker.mode();

		position = _coordinate_origin(nit_mode, config)
		         + Point(config.attribute_value("xpos", 0L),
		                 config.attribute_value("ypos", 0L));

		bool const attr = config.has_attribute("width") ||
		                  config.has_attribute("height");
		if (_initial_size.valid() && attr) {
			Genode::warning("setting both inital and normal attributes not "
			                " supported, ignore initial size");
			/* force initial to disable check below */
			_initial_size.set = true;
		}

		unsigned const nit_width  = nit_mode.width();
		unsigned const nit_height = nit_mode.height();

		long width  = config.attribute_value("width",  (long)nit_mode.width()),
		     height = config.attribute_value("height", (long)nit_mode.height());

		if (!_initial_size.set && _initial_size.valid()) {
			width  = _initial_size.width (nit_mode);
			height = _initial_size.height(nit_mode);

			_initial_size.set = true;
		} else {

			/*
			 * If configured width / height values are negative, the effective
			 * width / height is deduced from the screen size.
			 */
			if (width  < 0) width  = nit_width  + width;
			if (height < 0) height = nit_height + height;
		}

		fb_session.size(Area(width, height));
	}

	void handle_config_update()
	{
		config_rom.update();

		_update_size();

		update_view();
	}

	Signal_handler<Main> config_update_handler =
		{ env.ep(), *this, &Main::handle_config_update };

	void handle_mode_update()
	{
		_update_size();
	}

	Signal_handler<Main> mode_update_handler =
		{ env.ep(), *this, &Main::handle_mode_update };

	void handle_input()
	{
		Input::Event const * const events = input_ds.local_addr<Input::Event>();

		unsigned const num = nitpicker.input()->flush();
		bool update = false;

		for (unsigned i = 0; i < num; i++) {
			update |= events[i].focus_enter();
			input_session.submit(translate_event(events[i], position, fb_session.size()));
		}

		/* get to front if we got input focus */
		if (update)
			update_view();
	}

	Signal_handler<Main> input_handler =
		{ env.ep(), *this, &Main::handle_input };

	/**
	 * Constructor
	 */
	Main(Genode::Env &env) : env(env)
	{
		input_session.event_queue().enabled(true);

		/*
		 * Announce services
		 */
		env.parent().announce(env.ep().manage(fb_root));
		env.parent().announce(env.ep().manage(input_root));

		/*
		 * Apply initial configuration
		 */
		handle_config_update();

		/*
		 * Register signal handlers
		 */
		config_rom.sigh(config_update_handler);
		nitpicker.mode_sigh(mode_update_handler);
		nitpicker.input()->sigh(input_handler);
	}
};


void Component::construct(Genode::Env &env) { static Nit_fb::Main inst(env); }

