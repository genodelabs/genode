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
#include <base/id_space.h>

/* gems includes */
#include <gems/local_reporter.h>

/* local includes */
#include <types.h>

namespace Wm { class Window_registry; }


class Wm::Window_registry
{
	public:

		class Window;

		using Windows = Id_space<Window>;
		using Id      = Windows::Id;

		struct Alpha      { bool value; };
		struct Hidden     { bool value; };
		struct Resizeable { bool value; };

		class Window : Noncopyable
		{
			public:

				struct Attr
				{
					Gui::Title    title;
					Session_label label;
					Area          area;
					Alpha         alpha;
					Hidden        hidden;
					Resizeable    resizeable;

					bool operator == (Attr const &other) const
					{
						return title            == other.title
						    && label            == other.label
						    && area             == other.area
						    && alpha.value      == other.alpha.value
						    && hidden.value     == other.hidden.value
						    && resizeable.value == other.resizeable.value;
					}

					void gen_window_attr(Xml_generator &xml) const
					{
						xml.attribute("label",  label);
						xml.attribute("title",  title);
						xml.attribute("width",  area.w);
						xml.attribute("height", area.h);

						if (alpha.value)      xml.attribute("has_alpha",  "yes");
						if (hidden.value)     xml.attribute("hidden",     "yes");
						if (resizeable.value) xml.attribute("resizeable", "yes");
					}
				};

			private:

				Windows::Element _id;

				Attr         _attr;
				Attr mutable _flushed_attr { };

				friend class Window_registry;

				Window(Windows &windows, Id id, Attr const &attr)
				: _id(*this, windows, id), _attr(attr) { }

			public:

				Id id() const { return _id.id(); }

				/*
				 * Accessors for setting attributes
				 */
				void attr(Gui::Title const &title)    { _attr.title      = title; }
				void attr(Session_label const &label) { _attr.label      = label; }
				void attr(Area   area)                { _attr.area       = area;  }
				void attr(Alpha  alpha)               { _attr.alpha      = alpha; }
				void attr(Hidden hidden)              { _attr.hidden     = hidden; }
				void attr(Resizeable resizeable)      { _attr.resizeable = resizeable; }

				bool flushed() const { return _attr == _flushed_attr; }

				void generate_window_list_entry_xml(Xml_generator &xml) const
				{
					/*
					 * Skip windows that have no defined size, which may happen
					 * between the creation of a new window for a view w/o size
					 * and the first time when the top-level view's size is
					 * assigned.
					 */
					if (!_attr.area.valid())
						return;

					xml.node("window", [&] () {
						xml.attribute("id", id().value);
						_attr.gen_window_attr(xml);
					});
				}

				void mark_as_flushed() const { _flushed_attr = _attr; }
		};

		bool _flushed() const
		{
			bool result = true;
			_windows.for_each<Window const>([&] (Window const &w) {
				result &= w.flushed(); });
			return result;
		}

	private:

		Allocator          &_alloc;
		Expanding_reporter &_window_list_reporter;

		static constexpr unsigned MAX_WINDOWS = 1024;

		Bit_allocator<MAX_WINDOWS> _window_ids { };

		unsigned _next_id = 0; /* used to alloc subsequent numbers */

		Windows _windows { };

		void _with_window(Id id, auto const &fn, auto const &missing_fn)
		{
			_windows.apply<Window>(id, fn, missing_fn);
		}

		void _report_updated_window_list_model() const
		{
			_window_list_reporter.generate([&] (Xml_generator &xml) {
				_windows.for_each<Window>([&] (Window const &w) {
					w.generate_window_list_entry_xml(xml);
					w.mark_as_flushed();
				});
			});
		}

		void _set_attr(Id const id, auto const &value)
		{
			_with_window(id,
				[&] (Window &window) { window.attr(value); },
				[&] { warning("lookup for window ID ", id.value, " failed"); });
		}

	public:

		Window_registry(Allocator &alloc, Expanding_reporter &window_list_reporter)
		:
			_alloc(alloc), _window_list_reporter(window_list_reporter)
		{
			/* preserve ID 0 to represent an invalid ID */
			_window_ids.alloc();
		}

		enum class Create_error { IDS_EXHAUSTED };
		using Create_result = Attempt<Id, Create_error>;

		Create_result create(Window::Attr const &attr)
		{
			auto alloc_id = [&] () -> Create_result
			{
				for (unsigned i = 0; i < MAX_WINDOWS; i++) {
					unsigned try_id = _next_id;
					_next_id = (_next_id + 1) % MAX_WINDOWS;
					try {
						_window_ids.alloc_addr(try_id);
						return Id { try_id };
					}
					catch (...) { }
				}
				return Create_error::IDS_EXHAUSTED;
			};

			Create_result const result = alloc_id();

			result.with_result(
				[&] (Id id) {
					new (_alloc) Window(_windows, id, attr);
					_report_updated_window_list_model();
				},
				[&] (Create_error) { }
			);

			return result;
		}

		void destroy(Id id)
		{
			Window *win_ptr = nullptr;
			_with_window(id, [&] (Window &window) { win_ptr = &window; }, [&] { });

			if (!win_ptr)
				return;

			_window_ids.free(win_ptr->id().value);

			Genode::destroy(&_alloc, win_ptr);

			_report_updated_window_list_model();
		}

		void area (Id id, Area          const  area)  { _set_attr(id, area);  }
		void title(Id id, Gui::Title    const &title) { _set_attr(id, title); }
		void label(Id id, Session_label const &label) { _set_attr(id, label); }

		void alpha     (Id id, bool value) { _set_attr(id, Alpha      { value }); }
		void hidden    (Id id, bool value) { _set_attr(id, Hidden     { value }); }
		void resizeable(Id id, bool value) { _set_attr(id, Resizeable { value }); }

		void flush()
		{
			if (_flushed())
				return;

			_report_updated_window_list_model();
		}
};

#endif /* _WINDOW_REGISTRY_H_ */
