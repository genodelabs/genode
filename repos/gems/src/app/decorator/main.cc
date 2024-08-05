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

	Gui::Connection _gui { _env };

	struct Canvas
	{
		Framebuffer::Mode         const mode;
		Attached_dataspace              fb_ds;
		Decorator::Canvas<Pixel_rgb888> canvas;

		Canvas(Env &env, Gui::Connection &gui)
		:
			mode(gui.mode()),
			fb_ds(env.rm(),
			      (gui.buffer(mode, false), gui.framebuffer()->dataspace())),
			canvas(fb_ds.local_addr<Pixel_rgb888>(), mode.area, env.ram(), env.rm())
		{ }
	};

	Reconstructible<Canvas> _canvas { _env, _gui };

	Signal_handler<Main> _mode_handler { _env.ep(), *this, &Main::_handle_mode };

	void _handle_mode()
	{
		_canvas.construct(_env, _gui);

		_window_stack.mark_as_dirty(Rect(Point(0, 0), _canvas->mode.area));

		Dirty_rect dirty = _window_stack.draw(_canvas->canvas);

		dirty.flush([&] (Rect const &r) {
			_gui.framebuffer()->refresh(r.x1(), r.y1(), r.w(), r.h()); });
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

	/**
	 * Process the update every 'frame_period' GUI sync signals. The
	 * 'frame_cnt' holds the counter of the GUI sync signals.
	 *
	 * A lower 'frame_period' value makes the decorations more responsive
	 * but it also puts more load on the system.
	 *
	 * If the GUI sync signal fires every 10 milliseconds, a
	 * 'frame_period' of 2 results in an update rate of 1000/20 = 50 frames per
	 * second.
	 */
	unsigned _frame_cnt = 0;
	unsigned _frame_period = 2;

	/**
	 * Install handler for responding to GUI sync events
	 */
	void _handle_gui_sync();

	void _trigger_sync_handling()
	{
		_gui.framebuffer()->sync_sigh(_gui_sync_handler);
	}

	Signal_handler<Main> _gui_sync_handler = {
		_env.ep(), *this, &Main::_handle_gui_sync };

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

		_gui.mode_sigh(_mode_handler);

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

	_trigger_sync_handling();
}


void Decorator::Main::_handle_gui_sync()
{
	if (_frame_cnt++ < _frame_period)
		return;

	_frame_cnt = 0;

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

	/*
	 * To make the perceived animation speed independent from the setting of
	 * 'frame_period', we update the animation as often as the GUI
	 * sync signal occurs.
	 */
	for (unsigned i = 0; i < _frame_period; i++)
		_animator.animate();

	if (!model_updated && !windows_animated)
		return;

	Dirty_rect dirty = _window_stack.draw(_canvas->canvas);

	_window_stack.update_gui_views();

	_gui.execute();

	dirty.flush([&] (Rect const &r) {
		_gui.framebuffer()->refresh(r.x1(), r.y1(), r.w(), r.h()); });

	/*
	 * Disable sync handling when becoming idle
	 */
	if (!_animator.active())
		_gui.framebuffer()->sync_sigh(Signal_context_capability());
}


void Decorator::Main::_handle_pointer_update()
{
	_pointer.update();

	update_hover_report(_pointer.xml(), _window_stack, _hover, _hover_reporter);
}


void Component::construct(Genode::Env &env) { static Decorator::Main main(env); }
