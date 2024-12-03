/*
 * \brief  Window decorator that can be styled
 * \author Norman Feske
 * \date   2015-11-11
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>
#include <libc/component.h>
#include <base/heap.h>
#include <base/attached_rom_dataspace.h>
#include <os/reporter.h>
#include <timer_session/connection.h>

/* decorator includes */
#include <decorator/window_stack.h>
#include <decorator/xml_utils.h>

/* local includes */
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

	Constructible<Attached_rom_dataspace> _pointer { };

	Window_base::Hover _hover { };

	Reporter _hover_reporter = { _env, "hover" };

	/**
	 * GUI connection used to sync animations
	 */
	Gui::Connection _gui { _env };

	bool _window_layout_update_needed = false;

	Animator _animator { };

	Heap _heap { _env.ram(), _env.rm() };

	Theme _theme { _env.ram(), _env.rm(), _heap };

	Reporter _decorator_margins_reporter = { _env, "decorator_margins" };

	Ticks _previous_sync { };

	/**
	 * Install handler for responding to GUI sync events
	 */
	void _handle_gui_sync();

	Signal_handler<Main> _gui_sync_handler = {
		_env.ep(), *this, &Main::_handle_gui_sync };

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

	Attached_rom_dataspace _config { _env, "config" };

	Config _decorator_config { _config.xml() };

	void _handle_config();

	Signal_handler<Main> _config_handler = {
		_env.ep(), *this, &Main::_handle_config};

	/**
	 * Constructor
	 */
	Main(Env &env) : _env(env)
	{
		/*
		 * Eagerly upgrade the session quota in order to be able to create a
		 * high amount of view handles.
		 *
		 * XXX Consider upgrading the session quota on demand by responding
		 * to Out_of_ram or Out_of_caps exceptions raised by the create_view
		 * and view_handle operations. Currently, these exceptions will
		 * abort the decorator.
		 */
		_gui.upgrade_ram(256*1024);

		_config.sigh(_config_handler);
		_handle_config();

		_window_layout.sigh(_window_layout_handler);

		try {
			_pointer.construct(_env, "pointer");
			_pointer->sigh(_pointer_handler);
		} catch (Genode::Rom_connection::Rom_connection_failed) {
			Genode::log("pointer information unavailable");
		}

		_trigger_gui_sync();

		_hover_reporter.enabled(true);

		_decorator_margins_reporter.enabled(true);

		Genode::Reporter::Xml_generator xml(_decorator_margins_reporter, [&] ()
		{
			xml.node("floating", [&] () {

				Theme::Margins const margins = _theme.decor_margins();

				xml.attribute("top",    margins.top);
				xml.attribute("bottom", margins.bottom);
				xml.attribute("left",   margins.left);
				xml.attribute("right",  margins.right);
			});
		});

		/* import initial state */
		_handle_pointer_update();
		_handle_window_layout_update();
	}

	/**
	 * Window_factory_base interface
	 */
	Window_base *create(Xml_node window_node) override
	{
		return new (_heap)
			Window(_env, window_node.attribute_value("id", 0U),
			       _gui, _animator, _theme, _decorator_config);
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

	auto flush_window_stack_changes = [&] () {
		_window_stack.update_gui_views(); };

	if (_window_layout_update_needed) {

		_window_stack.update_model(_window_layout.xml(), flush_window_stack_changes);

		model_updated = true;

		/*
		 * A decorator element might have appeared or disappeared under
		 * the pointer.
		 */
		if (_pointer.constructed())
			update_hover_report(_pointer->xml(), _window_stack, _hover, _hover_reporter);

		_window_layout_update_needed = false;
	}

	bool const windows_animated = _window_stack.schedule_animated_windows();

	for (unsigned i = 0; i < passed_ticks.cs; i++)
		_animator.animate();

	if (model_updated || windows_animated) {
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
	if (!_pointer.constructed())
		return;

	_pointer->update();

	update_hover_report(_pointer->xml(), _window_stack, _hover, _hover_reporter);
}


void Libc::Component::construct(Libc::Env &env) { static Decorator::Main main(env); }
