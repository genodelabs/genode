/*
 * \brief  Handle timeouts which are late due to poor signal performance or
 *         due to scheduling overload
 * \author Alexander Boettcher
 * \date   2019-05-07
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 *
 * The code is partially based on the Seoul/Vancouver VMM, which is distributed
 * under the terms of the GNU General Public License version 2.
 *
 * Modifications by Intel Corporation are contributed under the terms and
 * conditions of the GNU General Public License version 2.
 */

#ifndef _TIMEOUT_LATE_H_
#define _TIMEOUT_LATE_H_

class Late_timeout
{
	public:

		struct Remote
		{
			timevalue _now      { 0 };
			timevalue _timeout  { 0 };
			unsigned  _timer_nr { ~0U };

			bool valid() const { return _timer_nr != ~0U; };
		};

	private:

		Genode::Lock _lock   { };
		Remote       _remote { };

	public:

		Late_timeout() { }

		void timeout(Clock &clock, MessageTimer const &msg)
		{
			Genode::Lock_guard<Genode::Lock> guard(_lock);

			Genode::uint64_t const now = clock.time();

			if (!_remote._now || now < _remote._now) {
				_remote._now      = now;
				_remote._timeout  = msg.abstime;
				_remote._timer_nr = msg.nr;
			}
		}

		Remote reset()
		{
			Genode::Lock_guard<Genode::Lock> guard(_lock);

			Remote last = _remote;

			_remote._now      = 0;
			_remote._timeout  = 0;
			_remote._timer_nr = ~0U;

			return last;
		}

		bool apply(Remote const &remote,
		           unsigned const timer_nr,
		           Genode::uint64_t now)
		{
			return (timer_nr == remote._timer_nr) &&
			       ((remote._timeout - remote._now) < (now - _remote._now));
		}
};

#endif /* _TIMEOUT_LATE_H_ */
