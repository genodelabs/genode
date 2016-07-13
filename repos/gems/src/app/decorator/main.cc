/*
 * \brief  Example window decorator that mimics the Motif look
 * \author Norman Feske
 * \date   2013-01-04
 */

/*
 * Copyright (C) 2013-2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/log.h>
#include <base/signal.h>
#include <nitpicker_session/connection.h>
#include <os/pixel_rgb565.h>
#include <os/attached_rom_dataspace.h>
#include <os/reporter.h>
#include <os/server.h>

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
	Server::Entrypoint &ep;

	Nitpicker::Connection nitpicker;

	Framebuffer::Mode mode = { nitpicker.mode() };

	Attached_dataspace fb_ds = { (nitpicker.buffer(mode, false),
	                              nitpicker.framebuffer()->dataspace()) };

	Canvas<Pixel_rgb565> canvas = { fb_ds.local_addr<Pixel_rgb565>(),
	                                Area(mode.width(), mode.height()) };

	Window_stack window_stack = { *this };

	/**
	 * Install handler for responding to window-layout changes
	 */
	void handle_window_layout_update(unsigned);

	Signal_rpc_member<Main> window_layout_dispatcher = {
		ep, *this, &Main::handle_window_layout_update };

	Attached_rom_dataspace window_layout { "window_layout" };

	/**
	 * Install handler for responding to pointer-position updates
	 */
	void handle_pointer_update(unsigned);

	Signal_rpc_member<Main> pointer_dispatcher = {
		ep, *this, &Main::handle_pointer_update };

	Attached_rom_dataspace pointer { "pointer" };

	Window_base::Hover hover;

	Reporter hover_reporter = { "hover" };

	bool window_layout_update_needed = false;

	Reporter decorator_margins_reporter = { "decorator_margins" };

	Animator animator;

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
	unsigned frame_cnt = 0;
	unsigned frame_period = 2;

	/**
	 * Install handler for responding to nitpicker sync events
	 */
	void handle_nitpicker_sync(unsigned);

	Signal_rpc_member<Main> nitpicker_sync_dispatcher = {
		ep, *this, &Main::handle_nitpicker_sync };

	Config config { *Genode::env()->heap() };

	void handle_config(unsigned);

	Signal_rpc_member<Main> config_dispatcher = {
		ep, *this, &Main::handle_config};

	/**
	 * Constructor
	 */
	Main(Server::Entrypoint &ep) : ep(ep)
	{
		Genode::config()->sigh(config_dispatcher);
		handle_config(0);

		window_layout.sigh(window_layout_dispatcher);
		pointer.sigh(pointer_dispatcher);

		nitpicker.framebuffer()->sync_sigh(nitpicker_sync_dispatcher);

		hover_reporter.enabled(true);

		decorator_margins_reporter.enabled(true);

		Genode::Reporter::Xml_generator xml(decorator_margins_reporter, [&] ()
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
		handle_pointer_update(0);
		handle_window_layout_update(0);
	}

	/**
	 * Window_factory_base interface
	 */
	Window_base *create(Xml_node window_node) override
	{
		for (unsigned retry = 0 ; retry < 2; retry ++) {
			try {
				return new (env()->heap())
					Window(attribute(window_node, "id", 0UL), nitpicker, animator, config);
			} catch (Nitpicker::Session::Out_of_metadata) {
				Genode::log("Handle Out_of_metadata of nitpicker session - upgrade by 8K");
				Genode::env()->parent()->upgrade(nitpicker.cap(), "ram_quota=8192");
			}
		}
		return 0;
	}

	/**
	 * Window_factory_base interface
	 */
	void destroy(Window_base *window) override
	{
		Genode::destroy(env()->heap(), static_cast<Window *>(window));
	}
};


void Decorator::Main::handle_config(unsigned)
{
	Genode::config()->reload();

	config.update();

	/* notify all windows to consider the updated policy */
	window_stack.for_each_window([&] (Window_base &window) {
		static_cast<Window &>(window).adapt_to_changed_config(); });

	/* trigger redraw of the window stack */
	handle_window_layout_update(0);
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


void Decorator::Main::handle_window_layout_update(unsigned)
{
	window_layout.update();

	window_layout_update_needed = true;
}


void Decorator::Main::handle_nitpicker_sync(unsigned)
{
	if (frame_cnt++ < frame_period)
		return;

	frame_cnt = 0;

	bool model_updated = false;

	if (window_layout_update_needed && window_layout.valid()) {

		try {
			Xml_node xml(window_layout.local_addr<char>(),
			             window_layout.size());
			window_stack.update_model(xml);

			model_updated = true;

			/*
			 * A decorator element might have appeared or disappeared under
			 * the pointer.
			 */
			if (pointer.valid())
				update_hover_report(Xml_node(pointer.local_addr<char>()),
				                    window_stack, hover, hover_reporter);

		} catch (Xml_node::Invalid_syntax) {

			/*
			 * An error occured with processing the XML model. Flush the
			 * internal representation.
			 */
			window_stack.flush();
		}

		window_layout_update_needed = false;
	}

	bool const windows_animated = window_stack.schedule_animated_windows();

	/*
	 * To make the perceived animation speed independent from the setting of
	 * 'frame_period', we update the animation as often as the nitpicker
	 * sync signal occurs.
	 */
	for (unsigned i = 0; i < frame_period; i++)
		animator.animate();

	if (!model_updated && !windows_animated)
		return;

	Dirty_rect dirty = window_stack.draw(canvas);

	window_stack.update_nitpicker_views();

	nitpicker.execute();

	dirty.flush([&] (Rect const &r) {
		nitpicker.framebuffer()->refresh(r.x1(), r.y1(), r.w(), r.h()); });
}


void Decorator::Main::handle_pointer_update(unsigned)
{
	pointer.update();

	if (pointer.valid())
		update_hover_report(Xml_node(pointer.local_addr<char>()),
		                    window_stack, hover, hover_reporter);
}


/************
 ** Server **
 ************/

namespace Server {

	char const *name() { return "decorator_ep"; }

	size_t stack_size() { return 8*1024*sizeof(long); }

	void construct(Entrypoint &ep)
	{
		static Decorator::Main main(ep);
	}
}
