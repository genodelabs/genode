/*
 * \brief  Focus history, used for swiching between recently focused windows
 * \author Norman Feske
 * \date   2016-02-02
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _FOCUS_HISTORY_H_
#define _FOCUS_HISTORY_H_

/* Genode includes */
#include <util/list.h>

namespace Window_layouter { class Focus_history; }


class Window_layouter::Focus_history
{
	public:

		struct Entry : List<Entry>::Element
		{
			Focus_history  &focus_history;
			Window_id const window_id;

			Entry(Focus_history &focus_history, Window_id window_id);

			inline ~Entry();
		};

	private:

		List<Entry> _entries { };

		Entry const *_next_in_cycle(Entry const &e) const
		{
			return e.next() ? e.next() : _entries.first();
		}

		Entry const *_prev_in_cycle(Entry const &e) const
		{
			auto last_ptr = [&]
			{
				Entry const *ptr = _entries.first();
				for ( ; ptr && ptr->next(); ptr = ptr->next());
				return ptr;
			};

			auto prev_ptr = [&] (Entry const &e) -> Entry const *
			{
				for (Entry const *ptr = _entries.first(); ptr; ptr = ptr->next())
					if (ptr->next() == &e)
						return ptr;
				return nullptr;
			};

			Entry const * const ptr = prev_ptr(e);
			return ptr ? ptr : last_ptr();
		}

		Window_id _any_suitable_or_none(auto const &cond_fn) const
		{
			for (Entry const *ptr = _entries.first(); ptr; ptr = ptr->next())
				if (cond_fn(ptr->window_id))
					return ptr->window_id;
			return Window_id();
		}

		Window_id _neighbor(Window_id window_id, auto const &cond_fn, auto const &next_fn) const
		{
			auto entry_ptr_for_window = [&] () -> Entry const *
			{
				for (Entry const *e = _entries.first(); e; e = e->next())
					if (e->window_id == window_id)
						return e;
				return nullptr;
			};

			Entry const * const anchor_ptr = entry_ptr_for_window();
			if (!anchor_ptr)
				return _any_suitable_or_none(cond_fn);

			Entry const *next_ptr = nullptr;
			for (Entry const *ptr = anchor_ptr; ptr; ptr = next_ptr) {
				next_ptr = next_fn(*ptr);

				bool const cycle_complete = (next_ptr == anchor_ptr);
				if (cycle_complete)
					return _any_suitable_or_none(cond_fn);

				if (next_ptr && cond_fn(next_ptr->window_id))
					return next_ptr->window_id;
			}
			return Window_id();
		}

	public:

		void focus(Window_id window_id)
		{
			if (window_id.value == 0)
				return;

			Entry *ptr = _entries.first();
			for (; ptr && ptr->window_id != window_id; ptr = ptr->next());

			if (!ptr) {
				warning("unexpected lookup failure for focus history entry");
				return;
			}

			_entries.remove(ptr);

			/* insert entry at the beginning (most recently focused) */
			_entries.insert(ptr);
		}

		Window_id next(Window_id id, auto const &cond_fn) const
		{
			return _neighbor(id, cond_fn, [&] (Entry const &e) { return _next_in_cycle(e); });
		}

		Window_id prev(Window_id id, auto const &cond_fn) const
		{
			return _neighbor(id, cond_fn, [&] (Entry const &e) { return _prev_in_cycle(e); });
		}
};


Window_layouter::Focus_history::Entry::Entry(Focus_history &focus_history,
                                             Window_id window_id)
:
	focus_history(focus_history), window_id(window_id)
{
	focus_history._entries.insert(this);
}


Window_layouter::Focus_history::Entry::~Entry()
{
	focus_history._entries.remove(this);
}

#endif /* _FOCUS_HISTORY_H_ */
