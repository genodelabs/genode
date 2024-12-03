/*
 * \brief  Example window decorator that mimics the Motif look
 * \author Norman Feske
 * \date   2013-01-04
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>
#include <base/component.h>
#include <base/heap.h>
#include <base/attached_rom_dataspace.h>
#include <gui_session/connection.h>
#include <timer_session/connection.h>
#include <os/pixel_rgb888.h>
#include <os/reporter.h>

/* decorator includes */
#include <decorator/window_stack.h>
#include <decorator/xml_utils.h>

/* local includes */
#include "canvas.h"
#include "window.h"


namespace Decorator {
	using namespace Genode;
	struct Main;
}


struct Decorator::Main : Window_factory_base
{
	Env &_env;

	Timer::Connection _timer { _env };

	/*
	 * Time base for animations, which are computed in steps of 10 ms
	 */
	struct Ticks { uint64_t cs; /* centi-seconds (10 ms) */ };

	Ticks _now()
	{
		return { .cs = _timer.curr_time().trunc_to_plain_ms().value / 10 };
	}

	Gui::Connection _gui { _env };

	struct Canvas
	{
		Env             &_env;
		Gui::Connection &_gui;

		Gui::Area const scr_area = _gui.panorama().convert<Gui::Area>(
			[&] (Gui::Rect rect) { return rect.area; },
			[&] (Gui::Undefined) { return Gui::Area { 1, 1 }; });

		/*
		 * The GUI connection's buffer is split into two parts. The upper
		 * part contains the front buffer displayed by the GUI server
		 * whereas the lower part contains the back buffer targeted by
		 * the Decorator::Canvas.
		 */
		Dataspace_capability _buffer_ds() const
		{
			_gui.buffer({ .area  = { .w = scr_area.w, .h = scr_area.h*2 },
			              .alpha = false });
			return _gui.framebuffer.dataspace();
		}

		Attached_dataspace fb_ds { _env.rm(), _buffer_ds() };

		Pixel_rgb888 *_canvas_pixels_ptr()
		{
			return fb_ds.local_addr<Pixel_rgb888>() + scr_area.count();
		}

		Decorator::Canvas<Pixel_rgb888> canvas {
			_canvas_pixels_ptr(), scr_area, _env.ram(), _env.rm() };

		Canvas(Env &env, Gui::Connection &gui) : _env(env), _gui(gui) { }
	};

	Reconstructible<Canvas> _canvas { _env, _gui };

	void _back_to_front(Dirty_rect dirty)
	{
		if (!_canvas.constructed())
			return;

		Rect const canvas_rect { { }, _canvas->scr_area };

		dirty.flush([&] (Rect const &r) {

			Rect  const clipped = Rect::intersect(r, canvas_rect);
			Point const from_p1 = clipped.p1() + Point { 0, int(canvas_rect.h()) };
			Point const to_p1   = clipped.p1();

			_gui.framebuffer.blit({ from_p1, clipped.area }, to_p1); });
	}

	Signal_handler<Main> _mode_handler { _env.ep(), *this, &Main::_handle_mode };

	void _handle_mode()
	{
		_canvas.construct(_env, _gui);

		_window_stack.mark_as_dirty(Rect(Point(0, 0), _canvas->scr_area));

		Dirty_rect dirty = _window_stack.draw(_canvas->canvas);

		_back_to_front(dirty);
	}

	Window_stack _window_stack = { *this };

	/**
	 * Handler for responding to window-layout changes
	 */
	void _handle_window_layout_update();

	Signal_handler<Main> _window_layout_handler = {
		_env.ep(), *this, &Main::_handle_window_layout_update };

	Attached_rom_dataspace _window_layout { _env, "window_layout" };

	/**
	 * Handler for responding to pointer-position updates
	 */
	void _handle_pointer_update();

	Signal_handler<Main> _pointer_handler = {
		_env.ep(), *this, &Main::_handle_pointer_update };

	Attached_rom_dataspace _pointer { _env, "pointer" };

	Window_base::Hover _hover { };

	Reporter _hover_reporter = { _env, "hover" };

	bool _window_layout_update_needed = false;

	Reporter _decorator_margins_reporter = { _env, "decorator_margins" };

	Animator _animator { };

	Ticks _previous_sync { };

	Signal_handler<Main> _gui_sync_handler = {
		_env.ep(), *this, &Main::_handle_gui_sync };

	void _handle_gui_sync();

	bool _gui_sync_enabled = false;

	void _trigger_gui_sync()
	{
		Ticks const now  = _now();
		bool  const idle = now.cs - _previous_sync.cs > 3;

		if (!_gui_sync_enabled || idle) {
			_previous_sync = now;
			_gui_sync_handler.local_submit();
		}
	}

	Heap _heap { _env.ram(), _env.rm() };

	Attached_rom_dataspace _config { _env, "config" };

	void _handle_config();

	Signal_handler<Main> _config_handler = {
		_env.ep(), *this, &Main::_handle_config};

	Config _decorator_config { _heap, _config.xml() };

	/**
	 * Constructor
	 */
	Main(Env &env) : _env(env)
	{
		_config.sigh(_config_handler);
		_handle_config();

		_gui.info_sigh(_mode_handler);

		_window_layout.sigh(_window_layout_handler);
		_pointer.sigh(_pointer_handler);

		_hover_reporter.enabled(true);

		_decorator_margins_reporter.enabled(true);

		Genode::Reporter::Xml_generator xml(_decorator_margins_reporter, [&] ()
		{
			xml.node("floating", [&] () {

				Window::Border const border = Window::border_floating();

				xml.attribute("top",    border.top);
				xml.attribute("bottom", border.bottom);
				xml.attribute("left",   border.left);
				xml.attribute("right",  border.right);
			});
		});

		/* import initial state */
		_handle_mode();
		_handle_pointer_update();
		_handle_window_layout_update();
	}

	/**
	 * Window_factory_base interface
	 */
	Window_base *create(Xml_node window_node) override
	{
		return new (_heap)
			Window(window_node.attribute_value("id", 0U),
			       _gui, _animator, _decorator_config);
	}

	/**
	 * Window_factory_base interface
	 */
	void destroy(Window_base *window) override
	{
		Genode::destroy(_heap, static_cast<Window *>(window));
	}
};


void Decorator::Main::_handle_config()
{
	_config.update();

	_decorator_config.update(_config.xml());

	/* notify all windows to consider the updated policy */
	_window_stack.for_each_window([&] (Window_base &window) {
		static_cast<Window &>(window).adapt_to_changed_config(); });

	/* trigger redraw of the window stack */
	_handle_window_layout_update();
}


static Decorator::Window_base::Hover
find_hover(Genode::Xml_node pointer_node, Decorator::Window_stack &window_stack)
{
	if (!pointer_node.has_attribute("xpos")
	 || !pointer_node.has_attribute("ypos"))
		return Decorator::Window_base::Hover();

	return window_stack.hover(Decorator::Point::from_xml(pointer_node));
}


static void update_hover_report(Genode::Xml_node pointer_node,
                                Decorator::Window_stack &window_stack,
                                Decorator::Window_base::Hover &hover,
                                Genode::Reporter &hover_reporter)
{
	Decorator::Window_base::Hover const new_hover =
		find_hover(pointer_node, window_stack);

	/* produce report only if hover state changed */
	if (new_hover != hover) {

		hover = new_hover;

		Genode::Reporter::Xml_generator xml(hover_reporter, [&] ()
		{
			if (hover.window_id > 0) {

				xml.node("window", [&] () {

					xml.attribute("id", hover.window_id);

					if (hover.left_sizer)   xml.node("left_sizer");
					if (hover.right_sizer)  xml.node("right_sizer");
					if (hover.top_sizer)    xml.node("top_sizer");
					if (hover.bottom_sizer) xml.node("bottom_sizer");
					if (hover.title)        xml.node("title");
					if (hover.closer)       xml.node("closer");
					if (hover.minimizer)    xml.node("minimizer");
					if (hover.maximizer)    xml.node("maximizer");
					if (hover.unmaximizer)  xml.node("unmaximizer");
				});
			}
		});
	}
}


void Decorator::Main::_handle_window_layout_update()
{
	_window_layout.update();

	_window_layout_update_needed = true;

	_trigger_gui_sync();
}


void Decorator::Main::_handle_gui_sync()
{
	Ticks const now = _now();

	Ticks const passed_ticks { now.cs - _previous_sync.cs };

	bool model_updated = false;

	auto flush_window_stack_changes = [&] () { };

	if (_window_layout_update_needed && _window_layout.valid()) {

		_window_stack.update_model(_window_layout.xml(), flush_window_stack_changes);

		model_updated = true;

		/*
		 * A decorator element might have appeared or disappeared under
		 * the pointer.
		 */
		update_hover_report(_pointer.xml(),
		                    _window_stack, _hover, _hover_reporter);

		_window_layout_update_needed = false;
	}

	bool const windows_animated = _window_stack.schedule_animated_windows();

	for (unsigned i = 0; i < passed_ticks.cs; i++)
		_animator.animate();

	if (model_updated || windows_animated) {

		Dirty_rect dirty = _window_stack.draw(_canvas->canvas);
		_back_to_front(dirty);

		_window_stack.update_gui_views();
		_gui.execute();
	}

	/*
	 * Enable/disable periodic sync depending on animation state
	 */
	if (_gui_sync_enabled) {
		if (!_animator.active()) {
			_gui.framebuffer.sync_sigh(Signal_context_capability());
			_gui_sync_enabled = false;
		}
	} else {
		if (_animator.active()) {
			_gui.framebuffer.sync_sigh(_gui_sync_handler);
			_gui_sync_enabled = true;
		}
	}

	_previous_sync = now;
}


void Decorator::Main::_handle_pointer_update()
{
	_pointer.update();

	update_hover_report(_pointer.xml(), _window_stack, _hover, _hover_reporter);
}


void Component::construct(Genode::Env &env) { static Decorator::Main main(env); }
