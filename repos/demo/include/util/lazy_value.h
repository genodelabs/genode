/*
 * \brief  Animated value
 * \author Norman Feske
 * \date   2010-10-29
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__UTIL__LAZY_VALUE_H_
#define _INCLUDE__UTIL__LAZY_VALUE_H_

template <typename T>
class Lazy_value
{
	private:

		T _speed;
		T _curr;
		T _dst;
		T _accel;

	public:

		Lazy_value()
		: _speed(0), _curr(0), _dst(0), _accel(1)
		{ }

		Lazy_value(T value)
		: _speed(0), _curr(value), _dst(value), _accel(1)
		{ }

		void dst(T dst, unsigned steps)
		{
			_dst = dst;
			T delta = (_curr > _dst) ? _curr - _dst : _dst - _curr;
			if (steps == 0)
				steps = 1;
			_accel = (4*delta) / (steps*steps);
			_speed = 0;
			if (_accel < 1)
				_accel = 1;
		}

		T dst() const { return _dst; }

		void assign(T value) { _curr = value; }

		void animate()
		{
			/* destination value reached? */
			if (_curr == _dst) {
				_speed = 0;
				return;
			}

			/* change value with current speed */
			if (_curr > _dst) {
				_curr -= _speed;
				if (_curr < _dst) _curr = _dst;
			} else {
				_curr += _speed;
				if (_curr > _dst) _curr = _dst;
			}

			/* adapt speed */
			T delta = _curr - _dst;
			if (delta < 0) delta = -delta;

			if (_speed*_speed < delta*_accel)
				_speed += _accel;
			else
				_speed -= _accel;

			if (_speed < 1) _speed = 1;
		}

		operator T () const { return _curr; }
};

#endif /* _INCLUDE__UTIL__LAZY_VALUE_H_ */
