/*
 * \brief   Browser interface
 * \date    2005-11-03
 * \author  Norman Feske <norman.feske@genode-labs.com>
 */

/*
 * Copyright (C) 2005-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _BROWSER_H_
#define _BROWSER_H_

#include "elements.h"
#include "history.h"

namespace Scout {
	extern Document *create_about();
	class Browser;
}


class Scout::Browser
{
	private:

		/*
		 * Noncopyable
		 */
		Browser(Browser const &);
		Browser &operator = (Browser const &);

	protected:

		Document *_document = nullptr;
		Document *_about;
		History   _history { };
		int       _voffset;
		int       _ypos = 0;

		/**
		 * Define content to present in browser window
		 */
		virtual void _content(Element *content) = 0;

		/**
		 * Request current content
		 */
		virtual Element *_content() = 0;

	public:

		/**
		 * Constructor
		 */
		explicit Browser(int voffset = 0)
		: _about(create_about()), _voffset(voffset) { }

		virtual ~Browser() { }

		/**
		 * Accessor functions
		 */
		virtual int ypos() { return _ypos; };

		/**
		 * Return current browser position
		 */
		virtual int view_x() { return 0; }
		virtual int view_y() { return 0; }

		/**
		 * Define vertical scroll offset of document
		 */
		virtual void ypos(int ypos) = 0;

		/**
		 * Format browser window
		 */
		virtual void format(Area) { }

		/**
		 * Travel backward in history
		 *
		 * \retval 1 success
		 * \retval 0 end of history is reached
		 */
		virtual int go_backward()
		{
			_history.assign(curr_anchor());
			if (!_history.go(History::BACKWARD)) return 0;
			go_to(_history.curr(), 0);
			return 1;
		}

		/**
		 * Follow history forward
		 *
		 * \retval 1 success,
		 * \retval 0 end of history is reached
		 */
		virtual int go_forward()
		{
			_history.assign(curr_anchor());
			if (!_history.go(History::FORWARD)) return 0;
			go_to(_history.curr(), 0);
			return 1;
		}

		/**
		 * Follow specified link location
		 *
		 * \param add_history  if set to 1, add new location to history
		 */
		virtual void go_to(Anchor *anchor, int add_history = 1)
		{
			if (!anchor) return;

			if (add_history) {
				_history.assign(curr_anchor());
				_history.add(anchor);
			}

			Element *new_content = anchor->chapter();
			if (new_content)
				_content(new_content);

			ypos(0);
			ypos(_ypos - anchor->abs_position().y() + _voffset);

			if (new_content) {
				new_content->refresh();
			}
		}

		/**
		 * Get current anchor
		 *
		 * The current anchor is the element that is visible at the
		 * top of the browser window. It depends on the scroll position.
		 * We need to store these elements in the history to recover
		 * the right viewport on the history entries even after
		 * reformatting the document.
		 */
		virtual Anchor *curr_anchor() = 0;

		/**
		 * Display table of contents
		 */
		int go_toc() { go_to(_document->toc, 1); return 1; }

		/**
		 * Go to title page
		 */
		int go_home() { go_to(_document); return 1; }

		/**
		 * Go to about page
		 */
		int go_about() { go_to(_about); return 1; }
};

#endif /* _BROWSER_H_ */
