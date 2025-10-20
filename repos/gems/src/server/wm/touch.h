/*
 * \brief  Global touch state tracker
 * \author Johannes Schlatow
 * \date   2026-01-09
 */

/*
 * Copyright (C) 2026 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TOUCH_H_
#define _TOUCH_H_

/* local includes */
#include <types.h>
#include <pointer.h>

namespace Wm { struct Touch; }


struct Wm::Touch
{
	using Position = Pointer::Position;
	using Tracker  = Pointer::Tracker;

	class State : Noncopyable
	{
		private:

			Position _last_touch_observed   { };
			Position _last_pointer_observed { };

			bool     _touched = false;

			Tracker &_tracker;

		public:

			State(Tracker &tracker) : _tracker(tracker) { }

			void apply_event(Input::Event const &ev)
			{
				bool pointer_report_update_needed = false;

				ev.handle_absolute_motion([&] (int x, int y) {
					Point const point { x, y };

					/* invalidate touch position when pointer moves */
					if (!_last_pointer_observed.valid || _last_pointer_observed.value != point)
						_last_touch_observed = { .valid = false, .value = { } };

					_last_pointer_observed = { .valid = true, .value = point };
				});

				/*
				 * update pointer report with touch position on BTN_TOUCH
				 * (i.e. after seq number update)
				 */
				if (ev.key_press(Input::BTN_TOUCH)) {
					_touched = true;
					pointer_report_update_needed = true;
				}

				if (ev.key_release(Input::BTN_TOUCH))
					_touched = false;

				/* update touch position before BTN_TOUCH key press */
				ev.handle_touch([&] (Input::Touch_id id, float x, float y) {
					if (id.value == 0 && !_touched)
						_last_touch_observed = { .valid = true, .value = { (int)x, (int)y }};
				});

				if (pointer_report_update_needed)
					_tracker.update_pointer_report();
			}

			Position last_observed_pos() const { return _last_touch_observed; }
	};
};

#endif /* _TOUCH_H_ */
