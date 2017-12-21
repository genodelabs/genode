/*
 * \brief   Timed event scheduler interface
 * \date    2005-10-24
 * \author  Norman Feske <norman.feske@genode-labs.com>
 */

/*
 * Copyright (C) 2005-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SCOUT__TICK_H_
#define _INCLUDE__SCOUT__TICK_H_

namespace Scout { class Tick; }


class Scout::Tick
{
	public:

		typedef unsigned long time;

	private:

		/*
		 * Noncopyable
		 */
		Tick(Tick const &);
		Tick &operator = (Tick const &);

		/**
		 * Tick object attributes
		 */
		time  _deadline = 0;          /* next deadline          */
		time  _period   = 0;          /* duration between ticks */
		Tick *_next     = nullptr;    /* next tick in tick list */
		int   _active   = 0;          /* set to one when active */

		/**
		 * Enqueue tick into tick queue
		 */
		void _enqueue();

		/**
		 * Dequeue tick from tick queue (for destruction of Tick)
		 */
		void _dequeue();

	protected:

		/**
		 * Function to be called on when deadline is reached
		 *
		 * This function must be implemented by a derived class.
		 * If the return value is 1, the tick is scheduled again.
		 */
		virtual int on_tick() = 0;

	public:

		Tick()
		{
			_deadline = 0;
			_period   = 0;
			_active   = 0;
			_next     = 0;
		}

		virtual ~Tick() { _dequeue(); }

		/**
		 * Schedule tick
		 *
		 * \param  tick period
		 */
		void schedule(time period);

		/**
		 * Return the number of scheduled ticks
		 */
		static int ticks_scheduled();

		/**
		 * Handle ticks
		 *
		 * \param now  current time
		 */
		static void handle(time now);
};


#endif /* _INCLUDE__SCOUT__TICK_H_ */
