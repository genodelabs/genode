/*
 * \brief  Timeout handling for netperf, based on test/alarm
 * \author Alexander Boettcher
 * \date   2014-01-10
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <pthread.h>
#include <unistd.h>
#include <base/log.h>

/* defined in "ports/contrib/netperf/src/netlib.c" */
extern "C" int times_up;

namespace {

	struct Timer_thread
	{
		pthread_t       _pthread { };
		pthread_attr_t  _attr    { };
		pthread_mutex_t _mutex   { PTHREAD_MUTEX_INITIALIZER };

		int _seconds_left = 0;

		void _entry()
		{
			for (;;) {
				sleep(1);

				pthread_mutex_lock(&_mutex);
				if (_seconds_left) {
					--_seconds_left;
					if (_seconds_left == 0) {
						times_up = true;
					}
				}
				pthread_mutex_unlock(&_mutex);
			}
		}

		static void *_entry(void *arg)
		{
			Timer_thread &myself = *(Timer_thread *)arg;
			myself._entry();

			return nullptr;
		}

		Timer_thread()
		{
			pthread_mutex_init(&_mutex, nullptr);
			pthread_create(&_pthread, &_attr, _entry, this);
		}

		void schedule_timeout(int seconds)
		{
			pthread_mutex_lock(&_mutex);
			times_up      = false;
			_seconds_left = seconds;
			pthread_mutex_unlock(&_mutex);
		}
	};
}


extern "C" void
start_timer(int time)
{
	static Timer_thread timer_thread { };

	timer_thread.schedule_timeout(time);
}
