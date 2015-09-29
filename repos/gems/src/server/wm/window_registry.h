/*
 * \brief  Window registry
 * \author Norman Feske
 * \date   2014-05-02
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _WINDOW_REGISTRY_H_
#define _WINDOW_REGISTRY_H_

/* Genode includes */
#include <util/bit_allocator.h>
#include <util/list.h>
#include <base/allocator.h>
#include <os/surface.h>
#include <os/reporter.h>
#include <os/session_policy.h>

/* gems includes */
#include <gems/local_reporter.h>

namespace Wm { class Window_registry; }


namespace Wm {
	using Genode::Allocator;
	using Genode::List;
	using Genode::Xml_generator;
	using Genode::Reporter;

	typedef Genode::Surface_base::Area  Area;
	typedef Genode::Surface_base::Point Point;
	typedef Genode::Surface_base::Rect  Rect;
}


class Wm::Window_registry
{
	public:

		struct Id
		{
			unsigned value;

			Id(unsigned value) : value(value) { }

			Id() /* invalid */ : value(0) { }

			bool operator == (Id const &other) const { return value == other.value; }

			bool valid() const { return value != 0; }
		};

		class Window : public List<Window>::Element
		{
			public:

				typedef Genode::String<200>   Title;
				typedef Genode::Session_label Session_label;

				enum Has_alpha { HAS_ALPHA, HAS_NO_ALPHA };

				enum Is_hidden { IS_HIDDEN, IS_NOT_HIDDEN };

				enum Resizeable { RESIZEABLE, NOT_RESIZEABLE };

			private:

				Id const _id;

				Title _title;

				Session_label _label;

				Area _size;

				Has_alpha _has_alpha = HAS_NO_ALPHA;

				Is_hidden _is_hidden = IS_NOT_HIDDEN;

				Resizeable _resizeable = NOT_RESIZEABLE;

				friend class Window_registry;

				Window(Id id) : _id(id) { }

			public:

				Id id() const { return _id; }

				/*
				 * Accessors for setting attributes
				 */
				void attr(Title const &title)         { _title = title; }
				void attr(Session_label const &label) { _label = label; }
				void attr(Area size)                  { _size  = size;  }
				void attr(Has_alpha has_alpha)        { _has_alpha = has_alpha; }
				void attr(Is_hidden is_hidden)        { _is_hidden = is_hidden; }
				void attr(Resizeable resizeable)      { _resizeable = resizeable; }

				void generate_window_list_entry_xml(Xml_generator &xml) const
				{
					xml.node("window", [&] () {
						xml.attribute("id",     _id.value);
						xml.attribute("label",  _label.string());
						xml.attribute("title",  _title.string());
						xml.attribute("width",  _size.w());
						xml.attribute("height", _size.h());

						if (_has_alpha == HAS_ALPHA)
							xml.attribute("has_alpha", "yes");

						if (_is_hidden == IS_HIDDEN)
							xml.attribute("hidden", "yes");

						if (_resizeable == RESIZEABLE)
							xml.attribute("resizeable", "yes");
					});
				}
		};

	private:

		Allocator &_alloc;
		Reporter  &_window_list_reporter;

		enum { MAX_WINDOWS = 1024 };

		Genode::Bit_allocator<MAX_WINDOWS> _window_ids;

		List<Window> _windows;

		Window *_lookup(Id id)
		{
			for (Window *w = _windows.first(); w; w = w->next())
				if (w->id() == id)
					return w;

			return 0;
		}

		void _report_updated_window_list_model() const
		{
			Reporter::Xml_generator xml(_window_list_reporter, [&] ()
			{
				for (Window const *w = _windows.first(); w; w = w->next())
					w->generate_window_list_entry_xml(xml);
			});
		}

		template <typename ATTR>
		void _set_attr(Id const id, ATTR const &value)
		{
			Window * const win = _lookup(id);

			if (!win) {
				PWRN("lookup for window ID %d failed", id.value);
				return;
			}

			win->attr(value);

			_report_updated_window_list_model();
		}

	public:

		Window_registry(Allocator &alloc, Reporter &window_list_reporter)
		:
			_alloc(alloc), _window_list_reporter(window_list_reporter)
		{
			/* preserve ID 0 to represent an invalid ID */
			_window_ids.alloc();
		}

		Id create()
		{
			Window * const win = new (_alloc) Window(_window_ids.alloc());

			_windows.insert(win);

			/*
			 * Even though we change the window-list model by adding a
			 * window, we don't call '_report_updated_window_list_model' here
			 * because the window does not have any useful properties before
			 * the 'size' function has been called.
			 *
			 * XXX should we pass the initial size as argument to this function?
			 */

			return win->id();
		}

		void destroy(Id id)
		{
			Window * const win = _lookup(id);
			if (!win)
				return;

			_windows.remove(win);

			Genode::destroy(&_alloc, win);

			_report_updated_window_list_model();
		}

		void size(Id id, Area size) { _set_attr(id, size); }

		void title(Id id, Window::Title title) { _set_attr(id, title); }

		void label(Id id, Window::Session_label label) { _set_attr(id, label); }

		void has_alpha(Id id, bool has_alpha)
		{
			_set_attr(id, has_alpha ? Window::HAS_ALPHA : Window::HAS_NO_ALPHA);
		}

		void is_hidden(Id id, bool is_hidden)
		{
			_set_attr(id, is_hidden ? Window::IS_HIDDEN : Window::IS_NOT_HIDDEN);
		}

		void resizeable(Id id, bool resizeable)
		{
			_set_attr(id, resizeable ? Window::RESIZEABLE : Window::NOT_RESIZEABLE);
		}
};

#endif /* _WINDOW_REGISTRY_H_ */
