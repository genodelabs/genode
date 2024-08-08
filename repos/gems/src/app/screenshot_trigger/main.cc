/*
 * \brief  Virtual print button
 * \author Norman Feske
 * \date   2023-01-29
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/env.h>
#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <gui_session/connection.h>
#include <event_session/connection.h>
#include <timer_session/connection.h>
#include <input/event.h>
#include <os/pixel_rgb888.h>
#include <nitpicker_gfx/box_painter.h>
#include <gems/gui_buffer.h>

namespace Screenshot_trigger {
	using namespace Genode;
	struct Main;
}


struct Screenshot_trigger::Main
{
	Env &_env;

	using Point = Gui_buffer::Point;
	using Area  = Gui_buffer::Area;
	using Rect  = Gui_buffer::Rect;

	unsigned _size     { };
	Point    _position { };
	Area     _area     { };

	Color const _color = Color::rgb(200, 0, 0);

	Input::Keycode const _keycode = Input::KEY_PRINT;

	uint64_t const _timeout_us = 1*1000*1000;

	Gui  ::Connection _gui   { _env };
	Event::Connection _event { _env };
	Timer::Connection _timer { _env };

	Constructible<Gui_buffer> _gui_buffer { };

	struct View
	{
		Gui::Connection &_gui;

		Gui::View_id _id { _gui.create_view() };

		View(Gui::Connection &gui, Point position, Area size) : _gui(gui)
		{
			using Command = Gui::Session::Command;
			_gui.enqueue<Command::Geometry>(_id, Rect(position, size));
			_gui.enqueue<Command::Front>(_id);
			_gui.execute();
		}

		~View() { _gui.destroy_view(_id); }
	};

	Constructible<View> _view { };

	Signal_handler<Main> _timer_handler { _env.ep(), *this, &Main::_handle_timer };
	Signal_handler<Main> _input_handler { _env.ep(), *this, &Main::_handle_input };

	/* used for hiding the view for a second after triggering */
	bool _visible = true;

	void visible(bool visible)
	{
		_visible = visible;
		_view.conditional(visible, _gui, _position, _area);
	}

	void _handle_input()
	{
		_gui.input.for_each_event([&] (Input::Event const &ev) {

			if (!_visible) /* ignore events while the view is invisble */
				return;

			bool const triggered = ev.key_release(Input::BTN_LEFT)
			                    || ev.touch_release();
			if (!triggered)
				return;

			/* hide trigger for some time */
			visible(false);
			_timer.trigger_once(_timeout_us);

			/* generate synthetic key-press-release sequence */
			_event.with_batch([&] (Event::Connection::Batch &batch) {
				batch.submit(Input::Press   { _keycode });
				batch.submit(Input::Release { _keycode });
			});
		});
	}

	void _handle_timer()
	{
		if (!_visible)
			visible(true);
	}

	void _render(Gui_buffer::Pixel_surface &pixel, Gui_buffer::Alpha_surface &alpha)
	{
		Box_painter::paint(pixel, Rect(Point(0, 0), _area), _color);

		long const half   = _size/2;
		long const max_sq = half*half;

		auto intensity = [&] (long x, long y)
		{
			x -= half,
			y -= half;

			long const r_sq = x*x + y*y;

			return 255 - min(255l, (r_sq*255)/max_sq);
		};

		/* fill alpha channel */
		Pixel_alpha8 *base = alpha.addr();
		for (unsigned y = 0; y < _area.h; y++)
			for (unsigned x = 0; x < _area.w; x++)
				*base++ = Pixel_alpha8 { 0, 0, 0, int(intensity(x, y)) };
	}

	Attached_rom_dataspace _config { _env, "config" };

	Signal_handler<Main> _config_handler { _env.ep(), *this, &Main::_handle_config };

	void _handle_config()
	{
		_config.update();

		Xml_node const config = _config.xml();

		_size     = config.attribute_value("size", 50u);
		_position = Point::from_xml(config);
		_area     = Area(_size, _size);

		_gui_buffer.construct(_gui, _area, _env.ram(), _env.rm());

		_gui_buffer->apply_to_surface([&] (auto &pixel, auto &alpha) {
			_render(pixel, alpha); });

		_gui_buffer->flush_surface();
	}

	Main(Env &env) : _env(env)
	{
		_config.sigh(_config_handler);
		_handle_config();

		_gui.input.sigh(_input_handler);
		_timer.sigh(_timer_handler);

		visible(true);
	}
};


void Component::construct(Genode::Env &env)
{
	static Screenshot_trigger::Main main(env);
}
