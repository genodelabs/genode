/*
 * \brief   Fading class
 * \date    2005-11-10
 * \author  Norman Feske <norman.feske@genode-labs.com>
 */

/*
 * Copyright (C) 2005-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SCOUT__FADER_H_
#define _INCLUDE__SCOUT__FADER_H_

#include <scout/misc_math.h>
#include <scout/tick.h>

namespace Scout { class Fader; }


/**
 * Class that manages the fading of a derived class.
 */
class Scout::Fader : public Tick
{
	protected:

		int _curr_value = 0;   /* current value       */
		int _dst_value  = 0;   /* desired final value */
		int _step       = 0;

	public:

		virtual ~Fader() { }

		/**
		 * Fade to specified alpha value
		 */
		void fade_to(int dst_value, int step = -1)
		{
			if (step > 0) _step = step;
			_dst_value  = dst_value;
			schedule(20);
		}

		/**
		 * Set fading speed
		 */
		void step(int step) { _step = step; }

		/**
		 * Assign new current fading value
		 */
		void curr(int curr)
		{
			if (curr == _curr_value) return;
			_curr_value = curr;
			schedule(20);
		}

		/**
		 * Tick interface
		 */
		int on_tick() override
		{
			if (_curr_value == _dst_value)
				return 0;

			if (_curr_value < _dst_value)
				_curr_value = min(_curr_value + _step, _dst_value);
			if (_curr_value > _dst_value)
				_curr_value = max(_curr_value - _step, _dst_value);

			return 1;
		}
};


#endif /* _INCLUDE__SCOUT__FADER_H_ */
