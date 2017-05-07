/*
 * \brief   Launchpad main program
 * \date    2006-08-30
 * \author  Norman Feske
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <base/heap.h>

#include <scout/platform.h>
#include <scout/tick.h>
#include <scout/user_state.h>
#include <scout/printf.h>
#include <scout/nitpicker_graphics_backend.h>

#include "config.h"
#include "elements.h"
#include "launchpad_window.h"


static Genode::Allocator *_alloc_ptr;

void *operator new (__SIZE_TYPE__ n) { return _alloc_ptr->alloc(n); }


using namespace Scout;
using namespace Genode;


/**
 * Facility to keep the available quota display up-to-date
 */
class Avail_quota_update : public Scout::Tick
{
	private:

		Ram_session &_ram;
		Launchpad   &_launchpad;
		size_t       _avail = 0;

	public:

		/**
		 * Constructor
		 */
		Avail_quota_update(Ram_session &ram, Launchpad &launchpad)
		:
			_ram(ram), _launchpad(launchpad)
		{
			schedule(200);
		}

		/**
		 * Tick interface
		 */
		int on_tick()
		{
			size_t new_avail = _ram.avail_ram().value;

			/* update launchpad window if needed */
			if (new_avail != _avail)
				_launchpad.quota(new_avail);

			_avail = new_avail;

			/* schedule next tick */
			return 1;
		}
};


struct Main : Scout::Event_handler
{
	Env &_env;

	Heap _heap { _env.ram(), _env.rm() };

	bool const _global_new_initialized = (_alloc_ptr = &_heap, true);

	Nitpicker::Connection _nitpicker { _env };

	Platform _platform { _env, *_nitpicker.input() };

	bool const _event_handler_registered = (_platform.event_handler(*this), true);

	Attached_rom_dataspace _config { _env, "config" };

	int      const _initial_x = _config.xml().attribute_value("xpos",   550U);
	int      const _initial_y = _config.xml().attribute_value("ypos",   150U);
	unsigned const _initial_w = _config.xml().attribute_value("width",  400U);
	unsigned const _initial_h = _config.xml().attribute_value("height", 400U);

	Scout::Area  const _max_size         { 530, 620 };
	Scout::Point const _initial_position { _initial_x, _initial_y };
	Scout::Area  const _initial_size     { _initial_w, _initial_h };

	Nitpicker_graphics_backend
		_graphics_backend { _env.rm(), _nitpicker, _heap, _max_size,
		                    _initial_position, _initial_size };

	Launchpad_window<Pixel_rgb565>
		_launchpad { _env, _graphics_backend, _initial_position, _initial_size,
		             _max_size, _env.ram().avail_ram().value };

	void _process_config()
	{
		try { _launchpad.process_config(_config.xml()); } catch (...) { }
	}

	bool const _config_processed = (_process_config(), true);

	Avail_quota_update _avail_quota_update { _env.ram(), _launchpad };

	User_state _user_state { &_launchpad, &_launchpad,
	                         _initial_position.x(), _initial_position.y() };

	void _init_launchpad()
	{
		_launchpad.parent(&_user_state);
		_launchpad.format(_initial_size);
		_launchpad.ypos(0);
	}

	bool const _launchpad_initialized = (_init_launchpad(), true);

	unsigned long _old_time = _platform.timer_ticks();

	void handle_event(Scout::Event const &event) override
	{
		using namespace Scout;

		Event ev = event;

		if (ev.type != Event::WHEEL)
			ev.mouse_position = ev.mouse_position - _user_state.view_position();

		_user_state.handle_event(ev);

		if (ev.type == Event::TIMER)
			Tick::handle(_platform.timer_ticks());

		/* perform periodic redraw */
		unsigned long const curr_time = _platform.timer_ticks();
		if (!_platform.event_pending() && ((curr_time - _old_time > 20)
		                               || (curr_time < _old_time))) {
			_old_time = curr_time;
			_launchpad.process_redraw();
		}
	}

	Main(Env &env) : _env(env) { }
};


void Component::construct(Genode::Env &env)
{
	/* XXX execute constructors of global statics */
	env.exec_static_constructors();

	static Main main(env);
}
