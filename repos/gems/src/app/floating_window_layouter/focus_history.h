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

namespace Floating_window_layouter { class Focus_history; }


class Floating_window_layouter::Focus_history
{
	public:

		struct Entry : Genode::List<Entry>::Element
		{
			Focus_history  &focus_history;
			Window_id const window_id;

			Entry(Focus_history &focus_history, Window_id window_id);

			inline ~Entry();
		};

	private:

		Genode::List<Entry> _entries;

		Entry *_lookup(Window_id window_id)
		{
			for (Entry *e = _entries.first(); e; e = e->next())
				if (e->window_id == window_id)
					return e;

			return nullptr;
		}

		void _remove_if_present(Entry &entry)
		{
			_entries.remove(&entry);
		}

	public:

		void focus(Window_id window_id)
		{
			Entry * const entry = _lookup(window_id);
			if (!entry) {
				Genode::warning("unexpected lookup failure for focus history entry");
				return;
			}

			_remove_if_present(*entry);

			/* insert entry at the beginning (most recently focused) */
			_entries.insert(entry);
		}

		Window_id next(Window_id window_id)
		{
			Entry * const first = _entries.first();
			if (!first)
				return Window_id();

			Entry * const entry = _lookup(window_id);
			if (!entry)
				return Window_id();

			Entry * const next  = entry->next();
			return next ? next->window_id : first->window_id;
		}

		Window_id prev(Window_id window_id)
		{
			Entry *curr = _entries.first();
			if (!curr)
				return Window_id();

			/* if argument refers to the first window, cycle to the last one */
			if (curr->window_id == window_id) {

				/* determine last list element */
				for (; curr->next(); curr = curr->next());
				return curr->window_id;
			}

			/* traverse list, looking for the predecessor of the window */
			for (; curr->next(); curr = curr->next())
				if (curr->next()->window_id == window_id)
					return curr->window_id;

			return Window_id();
		}
};


Floating_window_layouter::Focus_history::Entry::Entry(Focus_history &focus_history,
                                                      Window_id window_id)
:
	focus_history(focus_history), window_id(window_id)
{
	focus_history._entries.insert(this);
}


Floating_window_layouter::Focus_history::Entry::~Entry()
{
	focus_history._remove_if_present(*this);
}

#endif /* _FOCUS_HISTORY_H_ */
