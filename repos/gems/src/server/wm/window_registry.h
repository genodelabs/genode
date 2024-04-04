/*
 * \brief  Window registry
 * \author Norman Feske
 * \date   2014-05-02
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
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

				enum Hidden { HIDDEN, NOT_HIDDEN };

				enum Resizeable { RESIZEABLE, NOT_RESIZEABLE };

			private:

				Id const _id;

				struct Attr
				{
					Title         title { };
					Session_label label { };
					Area          size  { };
					Has_alpha     has_alpha = HAS_NO_ALPHA;
					Hidden        hidden = NOT_HIDDEN;
					Resizeable    resizeable = NOT_RESIZEABLE;

					bool operator == (Attr const &other) const
					{
						return title      == other.title
						    && label      == other.label
						    && size       == other.size
						    && has_alpha  == other.has_alpha
						    && hidden     == other.hidden
						    && resizeable == other.resizeable;
					}
				};

				Attr         _attr { };
				Attr mutable _flushed_attr { };

				friend class Window_registry;

				Window(Id id) : _id(id) { }

			public:

				Id id() const { return _id; }

				/*
				 * Accessors for setting attributes
				 */
				void attr(Title const &title)         { _attr.title      = title; }
				void attr(Session_label const &label) { _attr.label      = label; }
				void attr(Area size)                  { _attr.size       = size;  }
				void attr(Has_alpha has_alpha)        { _attr.has_alpha  = has_alpha; }
				void attr(Hidden hidden)              { _attr.hidden     = hidden; }
				void attr(Resizeable resizeable)      { _attr.resizeable = resizeable; }

				bool flushed() const { return _attr == _flushed_attr; }

				void generate_window_list_entry_xml(Xml_generator &xml) const
				{
					/*
					 * Skip windows that have no defined size, which happens
					 * between the creation of a new window and the first
					 * time when the window's properties are assigned.
					 */
					if (!_attr.size.valid())
						return;

					xml.node("window", [&] () {
						xml.attribute("id",     _id.value);
						xml.attribute("label",  _attr.label.string());
						xml.attribute("title",  _attr.title.string());
						xml.attribute("width",  _attr.size.w());
						xml.attribute("height", _attr.size.h());

						if (_attr.has_alpha == HAS_ALPHA)
							xml.attribute("has_alpha", "yes");

						if (_attr.hidden == HIDDEN)
							xml.attribute("hidden", "yes");

						if (_attr.resizeable == RESIZEABLE)
							xml.attribute("resizeable", "yes");
					});
				}

				void mark_as_flushed() const { _flushed_attr = _attr; }
		};

		bool _flushed() const
		{
			bool result = true;
			for (Window const *w = _windows.first(); w; w = w->next())
				result &= w->flushed();

			return result;
		}

	private:

		Allocator &_alloc;
		Reporter  &_window_list_reporter;

		static constexpr unsigned MAX_WINDOWS = 1024;

		Genode::Bit_allocator<MAX_WINDOWS> _window_ids { };

		unsigned _next_id = 0; /* used to alloc subsequent numbers */

		List<Window> _windows { };

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
				for (Window const *w = _windows.first(); w; w = w->next()) {
					w->generate_window_list_entry_xml(xml);
					w->mark_as_flushed();
				}
			});
		}

		template <typename ATTR>
		void _set_attr(Id const id, ATTR const &value)
		{
			Window * const win = _lookup(id);

			if (!win) {
				Genode::warning("lookup for window ID ", id.value, " failed");
				return;
			}

			win->attr(value);
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
			auto alloc_id = [&]
			{
				for (;;) {
					unsigned try_id = _next_id;
					_next_id = (_next_id + 1) % MAX_WINDOWS;
					try {
						_window_ids.alloc_addr(try_id);
						return try_id;
					}
					catch (...) { }
				}
			};

			Window * const win = new (_alloc) Window(alloc_id());

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

			_window_ids.free(win->id().value);

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

		void hidden(Id id, bool hidden)
		{
			_set_attr(id, hidden ? Window::HIDDEN : Window::NOT_HIDDEN);
		}

		void resizeable(Id id, bool resizeable)
		{
			_set_attr(id, resizeable ? Window::RESIZEABLE : Window::NOT_RESIZEABLE);
		}

		void flush()
		{
			if (_flushed())
				return;

			_report_updated_window_list_model();
		}
};

#endif /* _WINDOW_REGISTRY_H_ */
