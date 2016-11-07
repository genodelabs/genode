/*
 * \brief  Timer
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \author Norman Feske
 * \date   2014-10-10
 */

/*
 * Copyright (C) 2014-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _LX_KIT__TIMER_H_
#define _LX_KIT__TIMER_H_

/* Genode includes */
#include <os/server.h>

namespace Lx {

	class Timer;

	/**
	 * Return singleton instance of timer
	 *
	 * \param ep          entrypoint used handle timeout signals,
	 *                    to be specified at the first call of the function,
	 *                    which implicitly initializes the timer
	 * \param jiffies_ptr pointer to jiffies counter to be periodically
	 *                    updated
	 */
	Timer &timer(Server::Entrypoint *ep          = nullptr,
	             unsigned long      *jiffies_ptr = nullptr);

	void timer_update_jiffies();

	typedef unsigned long (*jiffies_update_func)(void);
	void register_jiffies_func(jiffies_update_func func);
}


class Lx::Timer
{
	public:

		enum Type { LIST, HR };
		/**
		 * Add new linux timer
		 */
		virtual void add(void *timer, Type type) = 0;

		/**
		 * Delete linux timer
		 */
		virtual int del(void *timer) = 0;

		/**
		 * Initial scheduling of linux timer
		 */
		virtual int schedule(void *timer, unsigned long expires) = 0;

		/**
		 * Schedule next linux timer
		 */
		virtual void schedule_next() = 0;

		/**
		 * Check if the timer is currently pending
		 */
		virtual bool pending(void const *timer) = 0;

		/**
		 * Check if the timer is already present
		 */
		virtual bool find(void const *timer) const = 0;

		/**
		 * Update jiffie counter
		 */
		virtual void update_jiffies() = 0;
};


#endif /* _LX_KIT__TIMER_H_ */
