/*
 * \brief  Utility for tracking dirty areas on a 2D coordinate space
 * \author Norman Feske
 * \date   2014-04-30
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__UTIL__DIRTY_RECT_H_
#define _INCLUDE__UTIL__DIRTY_RECT_H_

#include <base/stdint.h>

namespace Genode { template <typename, unsigned> class Dirty_rect; }


/**
 * Dirty-rectangle tracker
 *
 * \param RECT       rectangle type (as defined in 'util/geometry.h')
 * \param NUM_RECTS  number of rectangles used to represent the dirty area
 *
 */
template <typename RECT, unsigned NUM_RECTS>
class Genode::Dirty_rect
{
	private:

		typedef RECT Rect;
		typedef Genode::size_t size_t;

		Rect _rects[NUM_RECTS];

		/**
		 * Return true if it is worthwhile to merge 'r1' and 'r2' into one
		 */
		static bool _should_be_merged(Rect const &r1, Rect const &r2)
		{
			size_t const cnt_sum      = r1.area().count() + r2.area().count();
			size_t const cnt_compound = Rect::compound(r1, r2).area().count();

			return cnt_compound < cnt_sum;
		}

		/**
		 * Return the costs of adding a new to an existing rectangle
		 */
		static unsigned _costs(Rect const &existing, Rect const &added)
		{
			/*
			 * If 'existing' is unused, using it will cost the area of the
			 * added rectangle.
			 */
			if (!existing.valid())
				return added.area().count();

			/*
			 * If the existing rectangle is already populated, the costs
			 * correspond to the increase of the area when replacing the
			 * existing rectangle by the compound of the existing and new
			 * rectangles.
			 */
			return Rect::compound(existing, added).area().count()
			     - existing.area().count();
		}

	public:

		/**
		 * Call functor for each dirty area
		 *
		 * The functor 'fn' takes a 'Rect const &' as argument.
		 * This method resets the dirty rectangles.
		 */
		template <typename FN>
		void flush(FN const &fn)
		{
			/*
			 * Merge rectangles if their compound is smaller than sum of their
			 * areas. This happens if both rectangles overlap. In this case, it
			 * is cheaper to process the compound (including some portions that
			 * aren't actually dirty) instead of processing the overlap twice.
			 */
			for (unsigned i = 0; i < NUM_RECTS - 1; i++) {
				for (unsigned j = i + 1; j < NUM_RECTS; j++) {

					Rect &r1 = _rects[i];
					Rect &r2 = _rects[j];

					if (r1.valid() && r2.valid() && _should_be_merged(r1, r2)) {
						r1 = Rect::compound(r1, r2);
						r2 = Rect();
					}
				}
			}

			/*
			 * Apply functor to each dirty rectangle and mark rectangle as
			 * clear.
			 */
			for (unsigned i = 0; i < NUM_RECTS; i++) {
				if (_rects[i].valid()) {
					fn(_rects[i]);
					_rects[i] = Rect();
				}
			}
		}

		void mark_as_dirty(Rect added)
		{
			/* index of best matching rectangle in '_rects' array */
			unsigned best = 0;

			/* value to optimize */
			size_t highest_costs = ~0;

			/*
			 * Determine the most efficient rectangle to expand.
			 */
			for (unsigned i = 0; i < NUM_RECTS; i++) {

				size_t const costs = _costs(_rects[i], added);

				if (costs > highest_costs)
					continue;

				best          = i;
				highest_costs = costs;
			}

			Rect &rect = _rects[best];

			rect = rect.valid() ? Rect::compound(rect, added) : added;
		}
};

#endif /* _INCLUDE__UTIL__DIRTY_RECT_H_ */
