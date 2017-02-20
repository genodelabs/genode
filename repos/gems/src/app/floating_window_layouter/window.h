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

#ifndef _WINDOW_H_
#define _WINDOW_H_

/* local includes */
#include "types.h"
#include "focus_history.h"

namespace Floating_window_layouter { class Window; }


class Floating_window_layouter::Window : public List<Window>::Element
{
	public:

		typedef String<256> Title;
		typedef String<256> Label;

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

			Element(Type type) : type(type) { }

			bool operator != (Element const &other) const { return other.type != type; }
			bool operator == (Element const &other) const { return other.type == type; }
		};

	private:

		Window_id const _id;

		Title _title;

		Label _label;

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
		 * Backup of the original geometry while the window is maximized
		 */
		Rect _unmaximized_geometry;

		Rect const &_maximized_geometry;

		/** 
		 * Window may be partially transparent
		 */
		bool _has_alpha = false;

		/**
		 * Window is temporarily not visible
		 */
		bool _hidden = false;

		bool _resizeable = false;

		bool _maximized = false;

		bool _dragged = false;

		/*
		 * Number of times the window has been topped. This value is used by
		 * the decorator to detect the need for bringing the window to the
		 * front of nitpicker's global view stack even if the stacking order
		 * stays the same within the decorator instance. This is important in
		 * the presence of more than a single decorator.
		 */
		unsigned _topped_cnt = 0;

		Focus_history::Entry _focus_history_entry;

		bool _drag_left_border   = false;
		bool _drag_right_border  = false;
		bool _drag_top_border    = false;
		bool _drag_bottom_border = false;

		/**
		 * Called when the user starts dragging a window element
		 */
		void _initiate_drag_operation(Window::Element element)
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

			_dragged = true;
		}

		/**
		 * Called each time the pointer moves while the window is dragged
		 */
		void _apply_drag_operation(Point offset)
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

		/**
		 * Return true if user drags a window border
		 */
		bool _drag_border() const
		{
			return  _drag_left_border || _drag_right_border
			     || _drag_top_border  || _drag_bottom_border;
		}

	public:

		Window(Window_id id, Rect &maximized_geometry, Area initial_size,
		       Focus_history &focus_history)
		:
			_id(id),
			_requested_size(initial_size),
			_maximized_geometry(maximized_geometry),
			_focus_history_entry(focus_history, _id)
		{ }

		bool has_id(Window_id id) const { return id == _id; }

		Window_id id() const { return _id; }

		void title(Title const &title) { _title = title; }

		void label(Label const &label) { _label = label; }

		void geometry(Rect geometry) { _geometry = geometry; }

		Point position() const { return _geometry.p1(); }

		void position(Point pos) { _geometry = Rect(pos, _geometry.area()); }

		void has_alpha(bool has_alpha) { _has_alpha = has_alpha; }

		void hidden(bool hidden) { _hidden = hidden; }

		void resizeable(bool resizeable) { _resizeable = resizeable; }

		bool label_matches(Label const &label) const { return label == _label; }

		/**
		 * Define window size
		 *
		 * This function is called when the window-list model changes.
		 */
		void size(Area size)
		{
			if (_maximized) {
				_geometry = Rect(_maximized_geometry.p1(), size);
				return;
			}

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
			if (_hidden)
				return;

			xml.node("window", [&]() {

				xml.attribute("id", _id.value);

				/* present concatenation of label and title in the window's title bar */
				{
					bool const has_title = Genode::strlen(_title.string()) > 0;

					char buf[Label::capacity()];
					Genode::snprintf(buf, sizeof(buf), "%s%s%s",
					                 _label.string(),
					                 has_title ? " " : "",
					                 _title.string());

					xml.attribute("title",  buf);
				}

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
			_requested_size     = _geometry.area();
			_dragged         = false;
			_drag_left_border   = false;
			_drag_right_border  = false;
			_drag_top_border    = false;
			_drag_bottom_border = false;
		}

		void topped() { _topped_cnt++; }

		void close() { _requested_size = Area(0, 0); }

		bool maximized() const { return _maximized; }

		void maximized(bool maximized)
		{
			/* enter maximized state */
			if (!_maximized && maximized) {
				_unmaximized_geometry = _geometry;
				_requested_size = _maximized_geometry.area();
			}

			/* leave maximized state */
			if (_maximized && !maximized) {
				_requested_size = _unmaximized_geometry.area();
				_geometry = Rect(_unmaximized_geometry.p1(), _geometry.area());
			}

			_maximized = maximized;
		}
};

#endif /* _WINDOW_H_ */
