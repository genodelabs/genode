/*
 * \brief  Floating window layouter
 * \author Norman Feske
 * \date   2013-02-14
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>
#include <base/signal.h>
#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <base/heap.h>
#include <base/tslab.h>
#include <os/reporter.h>
#include <os/session_policy.h>
#include <nitpicker_session/connection.h>
#include <input_session/client.h>
#include <input/event.h>
#include <input/keycodes.h>
#include <decorator/xml_utils.h>

/* local includes */
#include "window.h"
#include "user_state.h"
#include "operations.h"

namespace Floating_window_layouter {

	struct Main;

	static Xml_node xml_lookup_window_by_id(Xml_node node, Window_id const id)
	{
		char const *tag     = "window";
		char const *id_attr = "id";

		for (node = node.sub_node(tag); ; node = node.next(tag))
			if (attribute(node, id_attr, 0UL) == id.value)
				return node;

		throw Xml_node::Nonexistent_sub_node();
	}


	/**
	 * Return true if compound XML node contains a sub node with ID
	 */
	static bool xml_contains_window_node_with_id(Xml_node node,
	                                             Window_id const id)
	{
		try { xml_lookup_window_by_id(node, id.value); return true; }
		catch (Xml_node::Nonexistent_sub_node) { return false; }
	}
}


struct Floating_window_layouter::Main : Operations
{
	Genode::Env &env;

	Genode::Attached_rom_dataspace config { env, "config" };

	Genode::Signal_handler<Main> config_dispatcher {
		env.ep(), *this, &Main::handle_config };

	Genode::Heap heap { env.ram(), env.rm() };

	Genode::Tslab<Window,4096> window_slab { &heap };

	List<Window> windows;

	Focus_history focus_history;

	void handle_config()
	{
		config.update();
	}

	Window *lookup_window_by_id(Window_id const id)
	{
		for (Window *w = windows.first(); w; w = w->next())
			if (w->has_id(id))
				return w;

		return nullptr;
	}

	Window const *lookup_window_by_id(Window_id const id) const
	{
		for (Window const *w = windows.first(); w; w = w->next())
			if (w->has_id(id))
				return w;

		return nullptr;
	}

	/**
	 * Apply functor to each window
	 *
	 * The functor is called with 'Window const &' as argument.
	 */
	template <typename FUNC>
	void for_each_window(FUNC const &fn) const
	{
		for (Window const *w = windows.first(); w; w = w->next())
			fn(*w);
	}

	User_state _user_state { *this, focus_history };


	/**************************
	 ** Operations interface **
	 **************************/

	void close(Window_id id) override
	{
		Window *window = lookup_window_by_id(id);
		if (!window)
			return;

		window->close();
		generate_resize_request_model();
		generate_focus_model();
	}

	void to_front(Window_id id) override
	{
		Window *window = lookup_window_by_id(id);
		if (!window)
			return;

		if (window != windows.first()) {
			windows.remove(window);
			windows.insert(window);
			window->topped();
			generate_window_layout_model();
		}
	}

	void focus(Window_id id) override
	{
		generate_window_layout_model();
		generate_focus_model();
	}

	void toggle_fullscreen(Window_id id) override
	{
		/* make sure that the specified window is the front-most one */
		to_front(id);

		Window *window = lookup_window_by_id(id);
		if (!window)
			return;

		window->maximized(!window->maximized());

		generate_resize_request_model();
	}

	void drag(Window_id id, Window::Element element, Point clicked, Point curr) override
	{
		to_front(id);

		Window *window = lookup_window_by_id(id);
		if (!window)
			return;

		/*
		 * The drag operation may result in a change of the window geometry.
		 * We detect such a change be comparing the original geometry with
		 * the geometry with the drag operation applied.
		 */
		Point const last_pos            = window->position();
		Area  const last_requested_size = window->requested_size();

		window->drag(element, clicked, curr);

		if (last_pos != window->position())
			generate_window_layout_model();

		if (last_requested_size != window->requested_size())
			generate_resize_request_model();
	}

	void finalize_drag(Window_id id, Window::Element, Point clicked, Point final)
	{
		Window *window = lookup_window_by_id(id);
		if (!window)
			return;

		window->finalize_drag_operation();

		/*
		 * Update window layout because highlighting may have changed after the
		 * drag operation. E.g., if the window has not kept up with the
		 * dragging of a resize handle, the resize handle is no longer hovered.
		 */
		generate_window_layout_model();
	}


	/**
	 * Install handler for responding to window-list changes
	 */
	void handle_window_list_update();

	Signal_handler<Main> window_list_dispatcher = {
		env.ep(), *this, &Main::handle_window_list_update };

	Attached_rom_dataspace window_list { env, "window_list" };


	/**
	 * Install handler for responding to focus requests
	 */
	void handle_focus_request_update();

	void _apply_focus_request();

	int handled_focus_request_id = 0;

	Signal_handler<Main> focus_request_dispatcher = {
		env.ep(), *this, &Main::handle_focus_request_update };

	Attached_rom_dataspace focus_request { env, "focus_request" };


	/**
	 * Install handler for responding to hover changes
	 */
	void handle_hover_update();

	Signal_handler<Main> hover_dispatcher = {
		env.ep(), *this, &Main::handle_hover_update };

	Attached_rom_dataspace hover { env, "hover" };


	/**
	 * Respond to decorator-margins information reported by the decorator
	 */
	Attached_rom_dataspace decorator_margins { env, "decorator_margins" };

	void handle_decorator_margins_update()
	{
		decorator_margins.update();

		/* respond to change by adapting the maximized window geometry */
		handle_mode_change();
	}

	Signal_handler<Main> decorator_margins_dispatcher = {
		env.ep(), *this, &Main::handle_decorator_margins_update };


	/**
	 * Install handler for responding to user input
	 */
	void handle_input()
	{
		while (input.pending())
			_user_state.handle_input(input_ds.local_addr<Input::Event>(),
			                         input.flush(), config.xml());
	}

	Signal_handler<Main> input_dispatcher {
		env.ep(), *this, &Main::handle_input };

	Nitpicker::Connection nitpicker { env };

	Rect maximized_window_geometry;

	void handle_mode_change()
	{
		/* determine maximized window geometry */
		Framebuffer::Mode const mode = nitpicker.mode();

		/* read decorator margins from the decorator's report */
		unsigned top = 0, bottom = 0, left = 0, right = 0;
		try {
			Xml_node const floating_xml = decorator_margins.xml().sub_node("floating");

			top    = attribute(floating_xml, "top",    0UL);
			bottom = attribute(floating_xml, "bottom", 0UL);
			left   = attribute(floating_xml, "left",   0UL);
			right  = attribute(floating_xml, "right",  0UL);

		} catch (...) { };

		maximized_window_geometry = Rect(Point(left, top),
		                                 Area(mode.width() - left - right,
		                                      mode.height() - top - bottom));
	}
	
	Signal_handler<Main> mode_change_dispatcher = {
		env.ep(), *this, &Main::handle_mode_change };


	Input::Session_client input { env.rm(), nitpicker.input_session() };

	Attached_dataspace input_ds { env.rm(), input.dataspace() };

	Reporter window_layout_reporter  = { env, "window_layout" };
	Reporter resize_request_reporter = { env, "resize_request" };
	Reporter focus_reporter          = { env, "focus" };


	bool focused_window_maximized() const
	{
		Window const *w = lookup_window_by_id(_user_state.focused_window_id());
		return w && w->maximized();
	}

	void import_window_list(Xml_node);
	void generate_window_layout_model();
	void generate_resize_request_model();
	void generate_focus_model();

	/**
	 * Constructor
	 */
	Main(Genode::Env &env) : env(env)
	{
		nitpicker.mode_sigh(mode_change_dispatcher);
		handle_mode_change();

		window_list.sigh(window_list_dispatcher);
		focus_request.sigh(focus_request_dispatcher);

		hover.sigh(hover_dispatcher);
		decorator_margins.sigh(decorator_margins_dispatcher);
		input.sigh(input_dispatcher);

		window_layout_reporter.enabled(true);
		resize_request_reporter.enabled(true);
		focus_reporter.enabled(true);

		/* import initial state */
		handle_window_list_update();

		/* attach update handler for config */
		config.sigh(config_dispatcher);
	}
};


void Floating_window_layouter::Main::import_window_list(Xml_node window_list_xml)
{
	char const *tag = "window";

	/*
	 * Remove windows from layout that are no longer in the window list
	 */
	for (Window *win = windows.first(), *next = 0; win; win = next) {
		next = win->next();
		if (!xml_contains_window_node_with_id(window_list_xml, win->id())) {
			windows.remove(win);
			destroy(window_slab, win);
		}
	}

	/*
	 * Update window attributes, add new windows to the layout
	 */
	try {
		for (Xml_node node = window_list_xml.sub_node(tag); ; node = node.next(tag)) {

			unsigned long id = 0;
			node.attribute("id").value(&id);

			Area const initial_size = area_attribute(node);

			Window *win = lookup_window_by_id(id);
			if (!win) {
				win = new (window_slab)
					Window(id, maximized_window_geometry, initial_size, focus_history);
				windows.insert(win);

				Point initial_position(150*id % 800, 30 + (100*id % 500));

				Window::Label const label = string_attribute(node, "label", Window::Label(""));
				win->label(label);

				/*
				 * Evaluate policy configuration for the window label
				 */
				try {
					Session_policy const policy(label, config.xml());

					if (policy.has_attribute("xpos") && policy.has_attribute("ypos"))
						initial_position = point_attribute(policy);

					win->maximized(policy.attribute_value("maximized", false));

				} catch (Genode::Session_policy::No_policy_defined) { }

				win->position(initial_position);
			}

			win->size(initial_size);
			win->title(string_attribute(node, "title", Window::Title("")));
			win->has_alpha( node.attribute_value("has_alpha",  false));
			win->hidden(    node.attribute_value("hidden",     false));
			win->resizeable(node.attribute_value("resizeable", false));
		}
	} catch (...) { }
}


void Floating_window_layouter::Main::generate_window_layout_model()
{
	Reporter::Xml_generator xml(window_layout_reporter, [&] ()
	{
		for (Window *w = windows.first(); w; w = w->next()) {

			bool const hovered = w->has_id(_user_state.hover_state().window_id);
			bool const focused = w->has_id(_user_state.focused_window_id());

			Window::Element const highlight =
				hovered ? _user_state.hover_state().element : Window::Element::UNDEFINED;

			w->serialize(xml, focused, highlight);
		}
	});
}


void Floating_window_layouter::Main::generate_resize_request_model()
{
	Reporter::Xml_generator xml(resize_request_reporter, [&] ()
	{
		for_each_window([&] (Window const &window) {

			Area const requested_size = window.requested_size();
			if (requested_size != window.size()) {
				xml.node("window", [&] () {
					xml.attribute("id",     window.id().value);
					xml.attribute("width",  requested_size.w());
					xml.attribute("height", requested_size.h());
				});
			}
		});
	});
}


void Floating_window_layouter::Main::generate_focus_model()
{
	Reporter::Xml_generator xml(focus_reporter, [&] ()
	{
		xml.node("window", [&] () {
			xml.attribute("id", _user_state.focused_window_id().value);
		});
	});
}


/**
 * Determine window element that corresponds to hover model
 */
static Floating_window_layouter::Window::Element
element_from_hover_model(Genode::Xml_node hover_window_xml)
{
	typedef Floating_window_layouter::Window::Element::Type Type;

	bool const left_sizer   = hover_window_xml.has_sub_node("left_sizer"),
	           right_sizer  = hover_window_xml.has_sub_node("right_sizer"),
	           top_sizer    = hover_window_xml.has_sub_node("top_sizer"),
	           bottom_sizer = hover_window_xml.has_sub_node("bottom_sizer");

	if (left_sizer && top_sizer)    return Type::TOP_LEFT;
	if (left_sizer && bottom_sizer) return Type::BOTTOM_LEFT;
	if (left_sizer)                 return Type::LEFT;

	if (right_sizer && top_sizer)    return Type::TOP_RIGHT;
	if (right_sizer && bottom_sizer) return Type::BOTTOM_RIGHT;
	if (right_sizer)                 return Type::RIGHT;

	if (top_sizer)    return Type::TOP;
	if (bottom_sizer) return Type::BOTTOM;

	if (hover_window_xml.has_sub_node("title"))     return Type::TITLE;
	if (hover_window_xml.has_sub_node("closer"))    return Type::CLOSER;
	if (hover_window_xml.has_sub_node("maximizer")) return Type::MAXIMIZER;
	if (hover_window_xml.has_sub_node("minimizer")) return Type::MINIMIZER;

	return Type::UNDEFINED;
}


void Floating_window_layouter::Main::handle_window_list_update()
{
	window_list.update();

	try {
		import_window_list(window_list.xml()); }
	catch (...) {
		Genode::error("could not import window list"); }

	generate_window_layout_model();
}


void Floating_window_layouter::Main::_apply_focus_request()
{
	Window::Label const label =
		focus_request.xml().attribute_value("label", Window::Label(""));

	int const id = focus_request.xml().attribute_value("id", 0L);

	/* don't apply the same focus request twice */
	if (id == handled_focus_request_id)
		return;

	bool focus_redefined = false;

	/*
	 * Move all windows that match the requested label to the front while
	 * maintaining their ordering.
	 */
	Window *at = nullptr;
	for (Window *w = windows.first(); w; w = w->next()) {

		if (!w->label_matches(label))
			continue;

		focus_redefined = true;

		/*
		 * Move window to behind the previous window that we moved to
		 * front. If 'w' is the first window that matches the selector,
		 * move it to the front ('at' argument of 'insert' is 0).
		 */
		windows.remove(w);
		windows.insert(w, at);

		/*
		 * Bring top-most window to the front of nitpicker's global view
		 * stack and set the focus to the top-most window.
		 */
		if (at == nullptr) {
			w->topped();

			_user_state.focused_window_id(w->id());
			generate_focus_model();
		}

		at = w;
	}

	if (focus_redefined)
		handled_focus_request_id = id;
}


void Floating_window_layouter::Main::handle_focus_request_update()
{
	focus_request.update();

	_apply_focus_request();

	generate_window_layout_model();
}


void Floating_window_layouter::Main::handle_hover_update()
{
	hover.update();

	try {
		Xml_node const hover_window_xml = hover.xml().sub_node("window");

		_user_state.hover(attribute(hover_window_xml, "id", 0UL),
		                  element_from_hover_model(hover_window_xml));
	}

	/*
	 * An exception may occur during the 'Xml_node' construction if the hover
	 * model is malformed. Under this condition, we invalidate the hover state.
	 */
	catch (...) {
		
		_user_state.reset_hover();

		/*
		 * Don't generate a focus-model update here. In a situation where the
		 * pointer has moved over a native nitpicker view (outside the realm of
		 * the window manager), the hover model as generated by the decorator
		 * naturally becomes empty. If we posted a focus update, this would
		 * steal the focus away from the native nitpicker view.
		 */
	}

	/* propagate changed hovering to the decorator */
	generate_window_layout_model();
}


/***************
 ** Component **
 ***************/

void Component::construct(Genode::Env &env) {
	static Floating_window_layouter::Main application(env); }
