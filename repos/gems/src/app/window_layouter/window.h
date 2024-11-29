/*
 * \brief  Window layouter
 * \author Norman Feske
 * \date   2013-02-14
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _WINDOW_H_
#define _WINDOW_H_

/* local includes */
#include <types.h>
#include <focus_history.h>
#include <assign.h>
#include <decorator_margins.h>

namespace Window_layouter { class Window; }


class Window_layouter::Window : public List_model<Window>::Element
{
	public:

		using Title = String<256>;
		using Label = String<256>;

		struct Element
		{
			enum Type { UNDEFINED, TITLE, LEFT, RIGHT, TOP, BOTTOM,
			            TOP_LEFT, TOP_RIGHT, BOTTOM_LEFT, BOTTOM_RIGHT,
			            CLOSER, MAXIMIZER, MINIMIZER };

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
				case CLOSER:       return "closer";
				case MAXIMIZER:    return "maximizer";
				case MINIMIZER:    return "minimizer";
				}
				return "";
			}

			static Element from_xml(Xml_node const &hover)
			{
				bool const left   = hover.has_sub_node("left_sizer"),
				           right  = hover.has_sub_node("right_sizer"),
				           top    = hover.has_sub_node("top_sizer"),
				           bottom = hover.has_sub_node("bottom_sizer");

				if (top && left)     return { TOP_LEFT     };
				if (bottom && left)  return { BOTTOM_LEFT  };
				if (left)            return { LEFT         };
				if (top && right)    return { TOP_RIGHT    };
				if (bottom && right) return { BOTTOM_RIGHT };
				if (right)           return { RIGHT        };
				if (top)             return { TOP          };
				if (bottom)          return { BOTTOM       };

				if (hover.has_sub_node("title"))     return { TITLE     };
				if (hover.has_sub_node("closer"))    return { CLOSER    };
				if (hover.has_sub_node("maximizer")) return { MAXIMIZER };
				if (hover.has_sub_node("minimizer")) return { MINIMIZER };

				return { UNDEFINED };
			}

			bool operator != (Element const &other) const { return other.type != type; }
			bool operator == (Element const &other) const { return other.type == type; }

			bool resize_handle() const
			{
				return type == LEFT        || type == RIGHT
				    || type == TOP         || type == BOTTOM
				    || type == TOP_LEFT    || type == TOP_RIGHT
				    || type == BOTTOM_LEFT || type == BOTTOM_RIGHT
				    || type == MAXIMIZER;
			}

			bool _any_of(Type t1, Type t2, Type t3) const
			{
				return type == t1 || type == t2 || type == t3;
			}

			bool left()   const { return _any_of(LEFT,   TOP_LEFT,    BOTTOM_LEFT);  }
			bool right()  const { return _any_of(RIGHT,  TOP_RIGHT,   BOTTOM_RIGHT); }
			bool top()    const { return _any_of(TOP,    TOP_LEFT,    TOP_RIGHT);    }
			bool bottom() const { return _any_of(BOTTOM, BOTTOM_LEFT, BOTTOM_RIGHT); }

			bool maximizer() const { return type == MAXIMIZER; }
			bool closer()    const { return type == CLOSER;    }
		};

		Window_id const id;

		Label const label;

	private:

		Title _title { };

		Decorator_margins const &_decorator_margins;

		Rect _geometry { };

		/**
		 * Window geometry at the start of the current drag operation
		 */
		Rect _orig_geometry { };

		/**
		 * Destined window geometry as defined by the user's drag operation
		 */
		Rect _drag_geometry { };

		Area _client_size;

		/**
		 * Size as desired by the user during resize drag operations
		 */
		Area _dragged_size;

		/**
		 * Target area the window can occupy, used while maximized
		 */
		Area _target_area { };

		/**
		 * Desired size to be requested to the client
		 */
		Area _requested_size() const
		{
			return (_maximized || !_floating)
			       ? _decorator_margins.inner_geometry({ { }, _target_area }).area
			       : _dragged_size;
		}

		/**
		 * Most recent resize request propagated to the window manager
		 *
		 * Initially, no resize request must be generated because the
		 * '_dragged_size' corresponds to the window size.
		 */
		Area _reported_resize_request = _dragged_size;

		/**
		 * Window may be partially transparent
		 */
		bool _has_alpha = false;

		/**
		 * Window is temporarily not visible
		 */
		bool _hidden = false;

		bool _resizeable = false;

		/**
		 * Toggled interactively
		 */
		bool _maximized = false;

		/**
		 * Set when position is defined in the window's assign rule
		 */
		bool _floating = false;

		bool _use_target_area() const { return _maximized || !_floating; }

		bool _dragged = false;

		Element _dragged_element { Element::UNDEFINED };

		bool _focused = false;

		Element _hovered { Element::UNDEFINED };

		/*
		 * Value that keeps track when the window has been moved to front
		 * most recently. It is used as a criterion for the order of the
		 * generated <assign> rules.
		 *
		 * This value is also used by the decorator to detect the need for
		 * bringing the window to the front of nitpicker's global view stack
		 * even if the stacking order stays the same within the decorator
		 * instance. This is important in the presence of more than a single
		 * decorator.
		 */
		unsigned _to_front_cnt = 0;

		Focus_history::Entry _focus_history_entry;

		struct Dragged_border
		{
			bool left, right, top, bottom;

			bool any() const { return left || right || top || bottom; };

		} _dragged_border { };

		/**
		 * Called when the user starts dragging a window element
		 */
		void _initiate_drag_operation(Window::Element element)
		{
			_dragged_element = element;

			if (_resizeable) {
				_dragged_border.left   = element.left();
				_dragged_border.right  = element.right();
				_dragged_border.top    = element.top();
				_dragged_border.bottom = element.bottom();
			}

			_orig_geometry = _geometry;
			_drag_geometry = _geometry;

			_dragged_size = _geometry.area;

			_dragged = true;
		}

		/**
		 * Called each time the pointer moves while the window is dragged
		 */
		void _apply_drag_operation(Point offset)
		{
			/* move window */
			if (!_dragged_border.any()) {
				_drag_geometry = Rect(_orig_geometry.p1() + offset,
				                      _orig_geometry.area);
				return;
			}

			/* resize window */
			int x1 = _orig_geometry.x1(), y1 = _orig_geometry.y1(),
			    x2 = _orig_geometry.x2(), y2 = _orig_geometry.y2();

			/* restrict resizing to the window's target area */
			Rect const outer { { }, _target_area };
			Rect const inner = _decorator_margins.inner_geometry(outer);

			auto clamped = [] (int v, int lowest, int highest) { return min(max(v, lowest), highest); };

			if (_dragged_border.left)   x1 = clamped(min(x1 + offset.x, x2), inner.x1(), outer.x2());
			if (_dragged_border.right)  x2 = clamped(max(x2 + offset.x, x1), outer.x1(), inner.x2());
			if (_dragged_border.top)    y1 = clamped(min(y1 + offset.y, y2), inner.y1(), outer.y2());
			if (_dragged_border.bottom) y2 = clamped(max(y2 + offset.y, y1), outer.y1(), inner.y2());

			_drag_geometry = Rect::compound(Point { x1, y1 }, Point { x2, y2 });

			_dragged_size = _drag_geometry.area;
		}

		Constructible<Assign::Member> _assign_member { };

	public:

		Window(Window_id id, Label const &label, Area initial_size,
		       Focus_history &focus_history,
		       Decorator_margins const &decorator_margins)
		:
			id(id), label(label),
			_decorator_margins(decorator_margins),
			_client_size(initial_size),
			_dragged_size(initial_size),
			_focus_history_entry(focus_history, id)
		{ }

		void title(Title const &title) { _title = title; }

		bool dragged() const { return _dragged; }

		Rect effective_inner_geometry() const
		{
			if (!_dragged)
				return _geometry;

			int x1 = _orig_geometry.x1(), y1 = _orig_geometry.y1(),
			    x2 = _orig_geometry.x2(), y2 = _orig_geometry.y2();

			/* move window */
			if (!_dragged_border.any())
				return _drag_geometry;

			/* resize window */
			if (_dragged_border.left)   x1 = x2 - _client_size.w + 1;
			if (_dragged_border.right)  x2 = x1 + _client_size.w - 1;
			if (_dragged_border.top)    y1 = y2 - _client_size.h + 1;
			if (_dragged_border.bottom) y2 = y1 + _client_size.h - 1;

			return Rect::compound(Point(x1, y1), Point(x2, y2));
		}

		/**
		 * Place window
		 *
		 * \param geometry  outer geometry available to the window
		 */
		void outer_geometry(Rect outer)
		{
			/* drop attempts to apply layout while dragging the window */
			if (_dragged)
				return;

			_geometry = _decorator_margins.inner_geometry(outer);

			_dragged_size = _geometry.area;
		}

		Rect outer_geometry() const
		{
			return _decorator_margins.outer_geometry(_geometry);
		}

		Area client_size() const { return _client_size; }

		void focused(bool focused) { _focused = focused; }

		void hovered(Element hovered) { _hovered = hovered; }

		Point position() const { return _geometry.p1(); }

		void has_alpha(bool has_alpha) { _has_alpha = has_alpha; }

		void hidden(bool hidden) { _hidden = hidden; }

		void resizeable(bool resizeable) { _resizeable = resizeable; }

		bool resizeable() const { return _resizeable; }

		/**
		 * Define window size
		 *
		 * This function is called when the window-list model changes.
		 */
		void client_size(Area size) { _client_size = size; }

		/**
		 * Return true if a request request to the window manager is due
		 */
		bool resize_request_needed() const
		{
			/* a resize request for the current size is already in flight */
			if (_requested_size() == _reported_resize_request)
				return false;

			return (_requested_size() != _client_size);
		}

		/**
		 * Mark the currently requested size as processed so that no further
		 * resize requests for the same size are generated
		 */
		void resize_request_updated() { _reported_resize_request = _requested_size(); }

		void gen_resize_request(Xml_generator &xml) const
		{
			Area const size = _requested_size();

			if (size == _client_size)
				return;

			xml.node("window", [&] () {
				xml.attribute("id",     id.value);
				xml.attribute("width",  size.w);
				xml.attribute("height", size.h);
			});
		}

		void generate(Xml_generator &xml, Rect const target_rect) const
		{
			/* omit window from the layout if hidden */
			if (_hidden)
				return;

			xml.node("window", [&]() {

				xml.attribute("id", id.value);

				/* present concatenation of label and title in the window's title bar */
				{
					bool const has_title = strlen(_title.string()) > 0;

					String<Label::capacity()> const
						title(label, (has_title ? " " : ""), _title);

					xml.attribute("title",  title);
				}

				Rect const rect = _use_target_area()
				                ? _decorator_margins.inner_geometry({ { }, _target_area })
				                : effective_inner_geometry();

				xml.attribute("xpos", rect.x1() + target_rect.x1());
				xml.attribute("ypos", rect.y1() + target_rect.y1());

				/*
				 * Constrain size of non-floating windows
				 *
				 * In contrast to a tiled or maximized window that is hard
				 * constrained by the size of the target geometry even if the
				 * client requests a larger size, floating windows respect the
				 * size defined by the client.
				 *
				 * This way, while floating, applications are able to resize
				 * their window according to the application's state. For
				 * example, a bottom-right resize corner of a Qt application is
				 * handled by the application, not the window manager, but it
				 * should still work as expected by the user. Note that the
				 * ability of the application to influence the layout is
				 * restricted to its window size. The window position cannot be
				 * influenced. This limits the ability of an application to
				 * impersonate other applications.
				 */
				Area const size = _use_target_area()
				                ? Area(min(rect.w(), _client_size.w),
				                       min(rect.h(), _client_size.h))
				                : _client_size;

				xml.attribute("width",  size.w);
				xml.attribute("height", size.h);

				if (_focused)
					xml.attribute("focused", "yes");

				if (_dragged) {

					xml.node("highlight", [&] () {
						xml.node(_dragged_element.name(), [&] () {
							xml.attribute("pressed", "yes"); }); });

				} else {

					bool const passive = (!_resizeable && _hovered.resize_handle())
					                  || (_hovered.type == Element::UNDEFINED);
					if (!passive)
						xml.node("highlight", [&] () {
							xml.node(_hovered.name()); });
				}

				if (_has_alpha)
					xml.attribute("has_alpha", "yes");

				if (_resizeable) {
					xml.attribute("maximizer", "yes");
					xml.attribute("closer", "yes");
				}
			});
		}

		void drag(Window::Element element, Point clicked, Point curr)
		{
			/* prevent maximized windows from being dragged */
			if (maximized())
				return;

			if (!_dragged)
				_initiate_drag_operation(element);

			_apply_drag_operation(curr - clicked);
		}

		void finalize_drag_operation()
		{
			_dragged_border = { };
			_geometry       = effective_inner_geometry();
			_dragged_size   = _geometry.area;
			_dragged        = false;
		}

		void to_front_cnt(unsigned to_front_cnt) { _to_front_cnt = to_front_cnt; }

		unsigned to_front_cnt() const { return _to_front_cnt; }

		void close() { _dragged_size = Area(0, 0); }

		void target_area(Area area) { _target_area = area; };

		void warp(Point const rel)
		{
			_geometry.at      = _geometry.at      + rel;
			_orig_geometry.at = _orig_geometry.at + rel;
			_drag_geometry.at = _drag_geometry.at + rel;
		}

		bool maximized() const { return _maximized; }

		void maximized(bool maximized) { _maximized = maximized; }

		void floating(bool floating) { _floating = floating; }

		void dissolve_from_assignment() { _assign_member.destruct(); }

		/**
		 * Associate window with a <assign> definition
		 */
		void assignment(Registry<Assign::Member> &registry)
		{
			/* retain first matching assignment only */
			if (_assign_member.constructed())
				return;

			_assign_member.construct(registry, *this);
		}

		/**
		 * List_model::Element
		 */
		bool matches(Xml_node const &node) const
		{
			return node.attribute_value("id", 0U) == id.value;
		}

		/**
		 * List_model::Element
		 */
		static bool type_matches(Xml_node const &) { return true; }
};

#endif /* _WINDOW_H_ */
