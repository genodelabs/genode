/*
 * \brief  Window decorator that can be styled
 * \author Norman Feske
 * \date   2015-11-11
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/log.h>
#include <base/component.h>
#include <base/heap.h>
#include <base/attached_rom_dataspace.h>
#include <os/reporter.h>

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

	Lazy_volatile_object<Attached_rom_dataspace> _pointer;

	Window_base::Hover _hover;

	Reporter _hover_reporter = { "hover" };

	/**
	 * Nitpicker connection used to sync animations
	 */
	Nitpicker::Connection _nitpicker { _env };

	bool _window_layout_update_needed = false;

	Animator _animator;

	Heap _heap { _env.ram(), _env.rm() };

	Theme _theme { _heap };

	Reporter _decorator_margins_reporter = { "decorator_margins" };

	/**
	 * Process the update every 'frame_period' nitpicker sync signals. The
	 * 'frame_cnt' holds the counter of the nitpicker sync signals.
	 *
	 * A lower 'frame_period' value makes the decorations more responsive
	 * but it also puts more load on the system.
	 *
	 * If the nitpicker sync signal fires every 10 milliseconds, a
	 * 'frame_period' of 2 results in an update rate of 1000/20 = 50 frames per
	 * second.
	 */
	unsigned _frame_cnt = 0;
	unsigned _frame_period = 2;

	/**
	 * Install handler for responding to nitpicker sync events
	 */
	void _handle_nitpicker_sync();

	Signal_handler<Main> _nitpicker_sync_handler = {
		_env.ep(), *this, &Main::_handle_nitpicker_sync };

	Attached_rom_dataspace _config { _env, "config" };

	Config _decorator_config;

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
		 * to Out_of_metadata exceptions raised by the create_view
		 * and view_handle operations. Currently, these exceptions will
		 * abort the decorator.
		 */
		_nitpicker.upgrade_ram(256*1024);

		_config.sigh(_config_handler);
		_handle_config();

		_window_layout.sigh(_window_layout_handler);

		try {
			_pointer.construct(_env, "pointer");
			_pointer->sigh(_pointer_handler);
		} catch (Genode::Rom_connection::Rom_connection_failed) {
			Genode::log("pointer information unavailable");
		}

		_nitpicker.framebuffer()->sync_sigh(_nitpicker_sync_handler);

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
			Window(attribute(window_node, "id", 0UL), _nitpicker, _animator,
			       _env.ram(), _theme, _decorator_config);
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

	return window_stack.hover(Decorator::point_attribute(pointer_node));
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
}


void Decorator::Main::_handle_nitpicker_sync()
{
	if (_frame_cnt++ < _frame_period)
		return;

	_frame_cnt = 0;

	bool model_updated = false;

	if (_window_layout_update_needed && _window_layout.valid()) {

		try {
			Xml_node xml(_window_layout.local_addr<char>(),
			             _window_layout.size());
			_window_stack.update_model(xml);

			model_updated = true;

			/*
			 * A decorator element might have appeared or disappeared under
			 * the pointer.
			 */
			if (_pointer.constructed() && _pointer->valid())
				update_hover_report(Xml_node(_pointer->local_addr<char>()),
				                    _window_stack, _hover, _hover_reporter);

		} catch (Xml_node::Invalid_syntax) {

			/*
			 * An error occured with processing the XML model. Flush the
			 * internal representation.
			 */
			_window_stack.flush();
		}

		_window_layout_update_needed = false;
	}

	bool const windows_animated = _window_stack.schedule_animated_windows();

	/*
	 * To make the perceived animation speed independent from the setting of
	 * 'frame_period', we update the animation as often as the nitpicker
	 * sync signal occurs.
	 */
	for (unsigned i = 0; i < _frame_period; i++)
		_animator.animate();

	if (!model_updated && !windows_animated)
		return;

	_window_stack.update_nitpicker_views();
}


void Decorator::Main::_handle_pointer_update()
{
	if (!_pointer.constructed())
		return;

	_pointer->update();

	if (_pointer->valid())
		update_hover_report(Xml_node(_pointer->local_addr<char>()),
		                    _window_stack, _hover, _hover_reporter);
}


void Component::construct(Genode::Env &env) { static Decorator::Main main(env); }
