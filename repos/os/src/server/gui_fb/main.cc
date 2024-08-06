/*
 * \brief  Framebuffer-to-GUI adapter
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
#include <gui_session/connection.h>
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

	using Point = Genode::Surface_base::Point;
	using Area  = Genode::Surface_base::Area;
	using Rect  = Genode::Surface_base::Rect;
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
		return Point(Genode::min((int)boundary.w - 1, Genode::max(0, p.x)),
		             Genode::min((int)boundary.h - 1, Genode::max(0, p.y))); };

	/* function to translate point to 'input_origin' */
	auto translate = [input_origin] (Point p) { return p - input_origin; };

	ev.handle_absolute_motion([&] (int x, int y) {
		Point p = clamp(translate(Point(x, y)));
		ev = Input::Absolute_motion{p.x, p.y};
	});

	ev.handle_touch([&] (Input::Touch_id id, float x, float y) {
		Point p = clamp(translate(Point((int)x, (int)y)));
		ev = Input::Touch{id, (float)p.x, (float)p.y};
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

	Gui::Connection &_gui;

	Genode::Signal_context_capability _mode_sigh { };

	Genode::Signal_context_capability _sync_sigh { };

	View_updater &_view_updater;

	/*
	 * Mode as requested by the configuration or by a mode change of our
	 * GUI session.
	 */
	Framebuffer::Mode _next_mode;

	using size_t = Genode::size_t;

	/*
	 * Number of bytes used for backing the current virtual framebuffer at
	 * the GUI server.
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
		             needed    = Gui::Session::ram_quota(mode, false),
		             usable    = _pd.avail_ram().value,
		             preserved = 64*1024;

		return used + usable > needed + preserved;
	}


	/**
	 * Constructor
	 */
	Session_component(Genode::Pd_session const &pd,
	                  Gui::Connection &gui,
	                  View_updater &view_updater,
	                  Framebuffer::Mode initial_mode)
	:
		_pd(pd), _gui(gui), _view_updater(view_updater),
		_next_mode(initial_mode)
	{ }

	void size(Gui::Area size)
	{
		/* ignore calls that don't change the size */
		if (Gui::Area(_next_mode.area.w, _next_mode.area.h) == size)
			return;

		Framebuffer::Mode const mode { .area = size };

		if (!_ram_suffices_for_mode(mode)) {
			Genode::warning("insufficient RAM for mode ", mode);
			return;
		}

		_next_mode = mode;

		if (_mode_sigh.valid())
			Genode::Signal_transmitter(_mode_sigh).submit();
	}

	Gui::Area size() const
	{
		return _active_mode.area;
	}


	/************************************
	 ** Framebuffer::Session interface **
	 ************************************/

	Genode::Dataspace_capability dataspace() override
	{
		_gui.buffer(_active_mode, false);

		_buffer_num_bytes =
			Genode::max(_buffer_num_bytes,
			            Gui::Session::ram_quota(_active_mode, false));

		/*
		 * We defer the update of the view until the client calls refresh the
		 * next time. This avoid showing the empty buffer as an intermediate
		 * artifact.
		 */
		_dataspace_is_new = true;

		return _gui.framebuffer.dataspace();
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

		_gui.framebuffer.refresh(x, y, w, h);
	}

	void sync_sigh(Genode::Signal_context_capability sigh) override
	{
		/*
		 * Keep a component-local copy of the signal capability. Otherwise,
		 * NOVA would revoke the capability from further recipients (in this
		 * case the GUI-server instance we are using) once we revoke the
		 * capability locally.
		 */
		_sync_sigh = sigh;

		_gui.framebuffer.sync_sigh(sigh);
	}
};


/******************
 ** Main program **
 ******************/

struct Nit_fb::Main : View_updater
{
	Genode::Env &env;

	Genode::Attached_rom_dataspace config_rom { env, "config" };

	Gui::Connection gui { env };

	Point position { 0, 0 };

	unsigned refresh_rate = 0;

	using View_handle = Gui::Session::View_handle;

	View_handle view = gui.create_view();

	Genode::Attached_dataspace input_ds { env.rm(), gui.input.dataspace() };

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
			if (_width > 0) return (unsigned)_width;
			if (_width < 0) return (unsigned)(mode.area.w + _width);
			return mode.area.w;
		}

		unsigned height(Framebuffer::Mode const &mode) const
		{
			if (_height > 0) return (unsigned)_height;
			if (_height < 0) return (unsigned)(mode.area.h + _height);
			return mode.area.h;
		}

		bool valid() const { return _width != 0 && _height != 0; }

	} _initial_size { config_rom.xml() };

	Framebuffer::Mode _initial_mode()
	{
		return Framebuffer::Mode { .area = { _initial_size.width (gui.mode()),
		                                     _initial_size.height(gui.mode()) } };
	}

	/*
	 * Input and framebuffer sessions provided to our client
	 */
	Input::Session_component       input_session { env, env.ram() };
	Framebuffer::Session_component fb_session { env.pd(), gui, *this, _initial_mode() };

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
		using Command = Gui::Session::Command;
		gui.enqueue<Command::Geometry>(view, Rect(position, fb_session.size()));
		gui.enqueue<Command::To_front>(view, View_handle());
		gui.execute();
	}

	/**
	 * Return screen-coordinate origin, depening on the config and screen mode
	 */
	static Point _coordinate_origin(Framebuffer::Mode mode, Xml_node config)
	{
		char const * const attr = "origin";

		if (!config.has_attribute(attr))
			return Point(0, 0);

		using Value = Genode::String<32>;
		Value const value = config.attribute_value(attr, Value());

		if (value == "top_left")     return Point(0, 0);
		if (value == "top_right")    return Point(mode.area.w, 0);
		if (value == "bottom_left")  return Point(0, mode.area.h);
		if (value == "bottom_right") return Point(mode.area.w, mode.area.h);

		warning("unsupported ", attr, " attribute value '", value, "'");
		return Point(0, 0);
	}

	void _update_size()
	{
		Xml_node const config = config_rom.xml();

		Framebuffer::Mode const gui_mode = gui.mode();

		position = _coordinate_origin(gui_mode, config) + Point::from_xml(config);

		bool const attr = config.has_attribute("width") ||
		                  config.has_attribute("height");
		if (_initial_size.valid() && attr) {
			Genode::warning("setting both inital and normal attributes not "
			                " supported, ignore initial size");
			/* force initial to disable check below */
			_initial_size.set = true;
		}

		unsigned const gui_width  = gui_mode.area.w;
		unsigned const gui_height = gui_mode.area.h;

		long width  = config.attribute_value("width",  (long)gui_mode.area.w),
		     height = config.attribute_value("height", (long)gui_mode.area.h);

		if (!_initial_size.set && _initial_size.valid()) {
			width  = _initial_size.width (gui_mode);
			height = _initial_size.height(gui_mode);

			_initial_size.set = true;
		} else {

			/*
			 * If configured width / height values are negative, the effective
			 * width / height is deduced from the screen size.
			 */
			if (width  < 0) width  = gui_width  + width;
			if (height < 0) height = gui_height + height;
		}

		fb_session.size(Area((unsigned)width, (unsigned)height));
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

		unsigned const num = gui.input.flush();
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
		gui.mode_sigh(mode_update_handler);
		gui.input.sigh(input_handler);
	}
};


void Component::construct(Genode::Env &env) { static Nit_fb::Main inst(env); }

