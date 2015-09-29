/*
 * \brief  Floating window layouter
 * \author Norman Feske
 * \date   2013-02-14
 */

/*
 * Copyright (C) 2013-2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <base/signal.h>
#include <os/attached_rom_dataspace.h>
#include <os/reporter.h>
#include <nitpicker_session/connection.h>
#include <input_session/client.h>
#include <input/event.h>
#include <input/keycodes.h>
#include <rom_session/connection.h>
#include <decorator/xml_utils.h>


namespace Floating_window_layouter {

	using namespace Genode;
	struct Main;
	class Window;

	typedef Decorator::Point Point;
	typedef Decorator::Area  Area;
	typedef Decorator::Rect  Rect;

	using Decorator::attribute;
	using Decorator::string_attribute;
	using Decorator::area_attribute;


	static Xml_node xml_lookup_window_by_id(Xml_node node, unsigned const id)
	{
		char const *tag     = "window";
		char const *id_attr = "id";

		for (node = node.sub_node(tag); ; node = node.next(tag))
			if (attribute(node, id_attr, 0UL) == id)
				return node;

		throw Xml_node::Nonexistent_sub_node();
	}


	/**
	 * Return true if compound XML node contains a sub node with ID
	 */
	static bool xml_contains_window_node_with_id(Xml_node node,
	                                             unsigned const id)
	{
		try { xml_lookup_window_by_id(node, id); return true; }
		catch (Xml_node::Nonexistent_sub_node) { return false; }
	}
}


class Floating_window_layouter::Window : public List<Window>::Element
{
	public:

		typedef String<256> Title;

		struct Element
		{
			enum Type { UNDEFINED, TITLE, LEFT, RIGHT, TOP, BOTTOM,
			            TOP_LEFT, TOP_RIGHT, BOTTOM_LEFT, BOTTOM_RIGHT };

			Type type;

			char const *name() const
			{
				switch (type) {
				case UNDEFINED:    return "";
				case TITLE:        return "title";
				case LEFT:         return "left";
				case RIGHT:        return "right";
				case TOP:          return "top";
				case BOTTOM:       return "bottom";
				case TOP_LEFT:     return "top_left";
				case TOP_RIGHT:    return "top_right";
				case BOTTOM_LEFT:  return "bottom_left";
				case BOTTOM_RIGHT: return "bottom_right";
				}
				return "";
			}

			Element(Type type) : type(type) { }

			bool operator != (Element const &other) const { return other.type != type; }
		};

	private:

		unsigned const _id = 0;

		Title _title;

		Rect _geometry;

		/**
		 * Window geometry at the start of the current drag operation
		 */
		Rect _orig_geometry;

		/**
		 * Size as desired by the user during resize drag operations
		 */
		Area _requested_size;

		/**
		 * Window may be partially transparent
		 */
		bool _has_alpha = false;

		/**
		 * Window is temporarily not visible
		 */
		bool _is_hidden = false;

		/*
		 * Number of times the window has been topped. This value is used by
		 * the decorator to detect the need for bringing the window to the
		 * front of nitpicker's global view stack even if the stacking order
		 * stays the same within the decorator instance. This is important in
		 * the presence of more than a single decorator.
		 */
		unsigned _topped_cnt = 0;

		bool _drag_left_border   = false;
		bool _drag_right_border  = false;
		bool _drag_top_border    = false;
		bool _drag_bottom_border = false;

	public:

		Window(unsigned id) : _id(id) { }

		bool has_id(unsigned id) const { return id == _id; }

		unsigned id() const { return _id; }

		void title(Title const &title) { _title = title; }

		void geometry(Rect geometry) { _geometry = geometry; }

		Point position() const { return _geometry.p1(); }

		void position(Point pos) { _geometry = Rect(pos, _geometry.area()); }

		void has_alpha(bool has_alpha) { _has_alpha = has_alpha; }

		void is_hidden(bool is_hidden) { _is_hidden = is_hidden; }

		/**
		 * Return true if user drags a window border
		 */
		bool _drag_border() const
		{
			return  _drag_left_border || _drag_right_border
			     || _drag_top_border  || _drag_bottom_border;
		}

		/**
		 * Define window size
		 *
		 * This function is called when the window-list model changes.
		 */
		void size(Area size)
		{
			if (!_drag_border()) {
				_geometry = Rect(_geometry.p1(), size);
				return;
			}

			Point p1 = _geometry.p1(), p2 = _geometry.p2();

			if (_drag_left_border)
				p1 = Point(p2.x() - size.w() + 1, p1.y());

			if (_drag_right_border)
				p2 = Point(p1.x() + size.w() - 1, p2.y());

			if (_drag_top_border)
				p1 = Point(p1.x(), p2.y() - size.h() + 1);

			if (_drag_bottom_border)
				p2 = Point(p2.x(), p1.y() + size.h() - 1);

			_geometry = Rect(p1, p2);
		}

		Area size() const { return _geometry.area(); }

		Area requested_size() const { return _requested_size; }

		void serialize(Xml_generator &xml, bool focused, Element highlight)
		{
			/* omit window from the layout if hidden */
			if (_is_hidden)
				return;

			xml.node("window", [&]() {
				xml.attribute("id",     _id);
				xml.attribute("title",  _title.string());
				xml.attribute("xpos",   _geometry.x1());
				xml.attribute("ypos",   _geometry.y1());
				xml.attribute("width",  _geometry.w());
				xml.attribute("height", _geometry.h());
				xml.attribute("topped", _topped_cnt);

				if (focused)
					xml.attribute("focused", "yes");

				if (highlight.type != Element::UNDEFINED) {
					xml.node("highlight", [&] () {
						xml.node(highlight.name());
					});
				}

				if (_has_alpha)
					xml.attribute("has_alpha", "yes");
			});
		}

		/**
		 * Called when the user starts dragging a window element
		 */
		void initiate_drag_operation(Window::Element element)
		{
			_drag_left_border   = (element.type == Window::Element::LEFT)
			                   || (element.type == Window::Element::TOP_LEFT)
			                   || (element.type == Window::Element::BOTTOM_LEFT);

			_drag_right_border  = (element.type == Window::Element::RIGHT)
			                   || (element.type == Window::Element::TOP_RIGHT)
			                   || (element.type == Window::Element::BOTTOM_RIGHT);

			_drag_top_border    = (element.type == Window::Element::TOP)
			                   || (element.type == Window::Element::TOP_LEFT)
			                   || (element.type == Window::Element::TOP_RIGHT);

			_drag_bottom_border = (element.type == Window::Element::BOTTOM)
			                   || (element.type == Window::Element::BOTTOM_LEFT)
			                   || (element.type == Window::Element::BOTTOM_RIGHT);

			_orig_geometry = _geometry;

			_requested_size = _geometry.area();
		}

		void apply_drag_operation(Point offset)
		{
			if (!_drag_border())
				position(_orig_geometry.p1() + offset);

			int requested_w = _orig_geometry.w(),
			    requested_h = _orig_geometry.h();

			if (_drag_left_border)   requested_w -= offset.x();
			if (_drag_right_border)  requested_w += offset.x();
			if (_drag_top_border)    requested_h -= offset.y();
			if (_drag_bottom_border) requested_h += offset.y();

			_requested_size = Area(max(1, requested_w), max(1, requested_h));
		}

		void finalize_drag_operation()
		{
			_requested_size = _geometry.area();
		}

		void topped() { _topped_cnt++; }
};


struct Floating_window_layouter::Main
{
	Signal_receiver &sig_rec;

	List<Window> windows;

	unsigned hovered_window_id = 0;
	unsigned focused_window_id = 0;
	unsigned key_cnt = 0;

	Window::Element hovered_element = Window::Element::UNDEFINED;

	bool drag_state     = false;
	bool drag_init_done = true;

	Window *lookup_window_by_id(unsigned id)
	{
		for (Window *w = windows.first(); w; w = w->next())
			if (w->has_id(id))
				return w;

		return nullptr;
	}


	/**
	 * Install handler for responding to window-list changes
	 */
	void handle_window_list_update(unsigned);

	Signal_dispatcher<Main> window_list_dispatcher = {
		sig_rec, *this, &Main::handle_window_list_update };

	Attached_rom_dataspace window_list { "window_list" };


	/**
	 * Install handler for responding to hover changes
	 */
	void handle_hover_update(unsigned);

	Signal_dispatcher<Main> hover_dispatcher = {
		sig_rec, *this, &Main::handle_hover_update };

	Attached_rom_dataspace hover { "hover" };

	/**
	 * Install handler for responding to user input
	 */
	void handle_input(unsigned);

	Signal_dispatcher<Main> input_dispatcher = {
		sig_rec, *this, &Main::handle_input };

	Nitpicker::Connection nitpicker;

	Input::Session_client input { nitpicker.input_session() };

	Attached_dataspace input_ds { input.dataspace() };

	Reporter window_layout_reporter  = { "window_layout" };
	Reporter resize_request_reporter = { "resize_request" };
	Reporter focus_reporter          = { "focus" };

	unsigned dragged_window_id = 0;

	Point pointer_clicked;
	Point pointer_last;
	Point pointer_curr;

	void import_window_list(Xml_node);
	void generate_window_layout_model();
	void generate_resize_request_model();
	void generate_focus_model();
	void initiate_window_drag(Window &window);

	/**
	 * Constructor
	 */
	Main(Signal_receiver &sig_rec) : sig_rec(sig_rec)
	{
		window_list.sigh(window_list_dispatcher);

		hover.sigh(hover_dispatcher);

		input.sigh(input_dispatcher);

		window_layout_reporter.enabled(true);
		resize_request_reporter.enabled(true);
		focus_reporter.enabled(true);
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
			destroy(env()->heap(), win);
		}
	}

	/*
	 * Update window attributes, add new windows to the layout
	 */
	try {
		for (Xml_node node = window_list_xml.sub_node(tag); ; node = node.next(tag)) {

			unsigned long id = 0;
			node.attribute("id").value(&id);

			Window *win = lookup_window_by_id(id);
			if (!win) {
				win = new (env()->heap()) Window(id);
				windows.insert(win);

				/*
				 * Define initial window position
				 */
				win->position(Point(150*id % 800, 30 + (100*id % 500)));
			}

			win->size(area_attribute(node));
			win->title(string_attribute(node, "title", Window::Title("untitled")));
			win->has_alpha(node.has_attribute("has_alpha")
			            && node.attribute("has_alpha").has_value("yes"));
			win->is_hidden(node.has_attribute("hidden")
			            && node.attribute("hidden").has_value("yes"));
		}
	} catch (...) { }
}


void Floating_window_layouter::Main::generate_window_layout_model()
{
	Reporter::Xml_generator xml(window_layout_reporter, [&] ()
	{
		for (Window *w = windows.first(); w; w = w->next()) {

			bool const is_hovered = w->has_id(hovered_window_id);
			bool const is_focused = w->has_id(focused_window_id);

			Window::Element const highlight =
				is_hovered ? hovered_element : Window::Element::UNDEFINED;

			w->serialize(xml, is_focused, highlight);
		}
	});
}


void Floating_window_layouter::Main::generate_resize_request_model()
{
	Reporter::Xml_generator xml(resize_request_reporter, [&] ()
	{
		Window const *dragged_window = lookup_window_by_id(dragged_window_id);
		if (dragged_window) {

			Area const requested_size = dragged_window->requested_size();
			if (requested_size != dragged_window->size()) {
				xml.node("window", [&] () {
					xml.attribute("id",     dragged_window_id);
					xml.attribute("width",  requested_size.w());
					xml.attribute("height", requested_size.h());
				});
			}
		}
	});
}


void Floating_window_layouter::Main::generate_focus_model()
{
	Reporter::Xml_generator xml(focus_reporter, [&] ()
	{
		xml.node("window", [&] () {
			xml.attribute("id", focused_window_id);
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

	if (hover_window_xml.has_sub_node("title")) return Type::TITLE;

	return Type::UNDEFINED;
}


void Floating_window_layouter::Main::initiate_window_drag(Window &window)
{
	window.initiate_drag_operation(hovered_element);

	drag_init_done = true;

	/* bring focused window to front */
	if (&window != windows.first()) {
		windows.remove(&window);
		windows.insert(&window);
	}

	window.topped();
}


void Floating_window_layouter::Main::handle_window_list_update(unsigned)
{
	window_list.update();

	try {
		import_window_list(Xml_node(window_list.local_addr<char>())); }
	catch (...) {
		PERR("Error while importing window list"); }

	generate_window_layout_model();
}


void Floating_window_layouter::Main::handle_hover_update(unsigned)
{
	hover.update();

	try {
		Xml_node const hover_xml(hover.local_addr<char>());

		Xml_node const hover_window_xml = hover_xml.sub_node("window");

		unsigned const id = attribute(hover_window_xml, "id", 0UL);

		Window::Element hovered = element_from_hover_model(hover_window_xml);

		/*
		 * Check if we have just received an update while already being in
		 * dragged state.
		 *
		 * This can happen when the user selects a new nitpicker domain by
		 * clicking on a window decoration. Prior the click, the new session is
		 * not aware of the current mouse position. So the hover model is not
		 * up to date. As soon as nitpicker assigns the focus to the new
		 * session and delivers the corresponding press event, we enter the
		 * drag state (in the 'handle_input' function. But we don't know which
		 * window is dragged until the decorator updates the hover model. Now,
		 * when the model is updated and we are still in dragged state, we can
		 * finally initiate the window-drag operation for the now-known window.
		 */
		if (id && !drag_init_done && dragged_window_id == 0)
		{
			dragged_window_id = id;
			hovered_window_id = id;
			focused_window_id = id;

			Window *window = lookup_window_by_id(id);
			if (window) {
				initiate_window_drag(*window);
				generate_window_layout_model();
				generate_focus_model();
			}
		}

		if (!drag_state && (id != hovered_window_id || hovered != hovered_element)) {

			hovered_window_id = id;
			hovered_element   = hovered;

			/* XXX read from config */
			bool const focus_follows_pointer = true;
			if (id && focus_follows_pointer) {
				focused_window_id = id;
				generate_focus_model();
			}

			generate_window_layout_model();
		}
	} catch (...) {

		/* reset focused window if pointer does not hover over any window */
		if (!drag_state) {
			hovered_element = Window::Element::UNDEFINED;
			hovered_window_id = 0;
			generate_window_layout_model();
			generate_focus_model();
		}
	}
}


void Floating_window_layouter::Main::handle_input(unsigned)
{
	bool need_regenerate_window_layout_model  = false;
	bool need_regenerate_resize_request_model = false;

	Window *hovered_window = lookup_window_by_id(hovered_window_id);

	while (input.is_pending()) {

		size_t const num_events = input.flush();

		Input::Event const * const ev = input_ds.local_addr<Input::Event>();

		for (size_t i = 0; i < num_events; i++) {

			Input::Event e = ev[i];

			if (e.type() == Input::Event::MOTION
			 || e.type() == Input::Event::FOCUS)
				pointer_curr = Point(e.ax(), e.ay());

			/* track number of pressed buttons/keys */
			if (e.type() == Input::Event::PRESS)   key_cnt++;
			if (e.type() == Input::Event::RELEASE) key_cnt--;

			if (e.type()    == Input::Event::PRESS
			 && e.keycode() == Input::BTN_LEFT) {

				drag_state        = true;
				drag_init_done    = false;
				dragged_window_id = hovered_window_id;
				pointer_clicked   = pointer_curr;
				pointer_last      = pointer_clicked;

				/*
				 * If the hovered window is known at the time of the press
				 * event, we can initiate the drag operation immediately.
				 * Otherwise, we the initiation is deferred to the next
				 * update of the hover model.
				 */
				if (hovered_window) {
					initiate_window_drag(*hovered_window);
					need_regenerate_window_layout_model = true;

					if (focused_window_id != hovered_window_id) {
						focused_window_id  = hovered_window_id;
						generate_focus_model();
					}
				}
			}

			/* detect end of drag operation */
			if (e.type() == Input::Event::RELEASE) {
				if (key_cnt == 0) {
					drag_state = false;
					generate_focus_model();

					Window *dragged_window = lookup_window_by_id(dragged_window_id);
					if (dragged_window) {

						Area const last_requested_size = dragged_window->requested_size();
						dragged_window->finalize_drag_operation();

						if (last_requested_size != dragged_window->requested_size())
							need_regenerate_resize_request_model = true;

						/*
						 * Update window layout because highlighting may have
						 * changed after the drag operation. E.g., if the
						 * window has not kept up with the dragging of a
						 * resize handle, the resize handle is no longer
						 * hovered.
						 */
						handle_hover_update(0);
					}
				}
			}
		}
	}

	if (drag_state && (pointer_curr != pointer_last)) {

		pointer_last = pointer_curr;

		Window *dragged_window = lookup_window_by_id(dragged_window_id);
		if (dragged_window) {

			Point const last_pos            = dragged_window->position();
			Area  const last_requested_size = dragged_window->requested_size();

			dragged_window->apply_drag_operation(pointer_curr - pointer_clicked);

			if (last_pos != dragged_window->position())
				need_regenerate_window_layout_model = true;

			if (last_requested_size != dragged_window->requested_size())
				need_regenerate_resize_request_model = true;
		}
	}

	if (need_regenerate_window_layout_model)
		generate_window_layout_model();

	if (need_regenerate_resize_request_model)
		generate_resize_request_model();
}


int main(int argc, char **argv)
{
	static Genode::Signal_receiver sig_rec;

	static Floating_window_layouter::Main application(sig_rec);

	/* import initial state */
	application.handle_window_list_update(0);

	/* process incoming signals */
	for (;;) {
		using namespace Genode;

		Signal sig = sig_rec.wait_for_signal();
		Signal_dispatcher_base *dispatcher =
			dynamic_cast<Signal_dispatcher_base *>(sig.context());

		if (dispatcher)
			dispatcher->dispatch(sig.num());
	}
}
