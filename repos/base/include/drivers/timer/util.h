/*
 * \brief   Utilities for timer drivers
 * \author  Martin Stein
 * \date    2017-08-23
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DRIVERS__TIMER__UTIL_H_
#define _DRIVERS__TIMER__UTIL_H_

namespace Genode {

	enum { TIMER_MIN_TICKS_PER_MS = 1000ULL };

	/*
	 * Translate timer ticks to microseconds without losing precicision
	 *
	 * There are hardware timers whose frequency can't be expressed as
	 * ticks-per-microsecond integer-value because only a ticks-per-millisecond
	 * integer-value is precise enough. We don't want to use expensive
	 * floating-point values here but nonetheless want to translate from ticks
	 * to time with microseconds precision. Thus, we split the input in two and
	 * translate both parts separately. This way, we can raise precision by
	 * shifting the values to their optimal bit position. Afterwards, the
	 * results are shifted back and merged together again.
	 *
	 * Please ensure that the assertion
	 * "ticks_per_ms >= TIMER_MIN_TICKS_PER_MS" is true when calling this
	 * method!
	 */
	template <typename RESULT_T, typename TICS_PER_MS_T>
	RESULT_T timer_ticks_to_us(RESULT_T      const ticks,
	                           TICS_PER_MS_T const ticks_per_ms)
	{
		enum:RESULT_T {
			HALF_WIDTH = (sizeof(RESULT_T) << 2),
			MSB_MASK  = (~(RESULT_T)0) << HALF_WIDTH,
			LSB_MASK  = (~(RESULT_T)0) >> HALF_WIDTH,
			MSB_RSHIFT = 10,
			LSB_LSHIFT = HALF_WIDTH - MSB_RSHIFT,
		};
		RESULT_T const msb = ((((ticks & MSB_MASK) >> MSB_RSHIFT)
		                       * 1000) / ticks_per_ms) << MSB_RSHIFT;
		RESULT_T const lsb = ((((ticks & LSB_MASK) << LSB_LSHIFT)
		                       * 1000) / ticks_per_ms) >> LSB_LSHIFT;
		return msb + lsb;
	}
}

#endif /* _DRIVERS__TIMER__UTIL_H_ */
