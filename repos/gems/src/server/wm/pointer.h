/*
 * \brief  Type of client that receive the last motion event
 * \author Norman Feske
 * \date   2014-06-10
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _POINTER_H_
#define _POINTER_H_

/* local includes */
#include <types.h>

namespace Wm { struct Pointer; }


struct Wm::Pointer
{
	struct Position
	{
		bool  valid;
		Point value;
	};


	struct Tracker : Interface, Noncopyable
	{
		virtual void update_pointer_report() = 0;
	};


	class State : Noncopyable
	{
		private:

			Position _last_observed { };

			unsigned _key_cnt = 0;

			Tracker &_tracker;

		public:

			State(Tracker &tracker) : _tracker(tracker) { }

			void apply_event(Input::Event const &ev)
			{
				bool pointer_report_update_needed = false;

				if (ev.hover_leave())
					_last_observed = { .valid = false, .value = { } };

				ev.handle_absolute_motion([&] (int x, int y) {
					_last_observed = { .valid = true, .value = { x, y } }; });

				if (ev.absolute_motion() || ev.hover_leave())
					pointer_report_update_needed = true;

				if (ev.press())
					_key_cnt++;

				if (ev.release()) {
					_key_cnt--;

					/*
					 * When returning from a drag operation to idle state, the
					 * pointer position may have moved to another window
					 * element. Propagate the least recent pointer position to
					 * the decorator to update its hover model.
					 */
					if (_key_cnt == 0)
						pointer_report_update_needed = true;
				}

				if (pointer_report_update_needed)
					_tracker.update_pointer_report();
			}

			Position last_observed_pos() const { return _last_observed; }
	};
};

#endif /* _POINTER_H_ */
