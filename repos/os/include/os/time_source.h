/*
 * \brief  Interface of a time source that can handle one timeout at a time
 * \author Martin Stein
 * \date   2016-11-04
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _OS__TIME_SOURCE_H_
#define _OS__TIME_SOURCE_H_

/* Genode includes */
#include <os/duration.h>

namespace Genode {

	class Time_source;
	class Timeout_scheduler;
}


/**
 * Interface of a time source that can handle one timeout at a time
 */
struct Genode::Time_source
{
	/**
	 * Interface of a timeout callback
	 */
	struct Timeout_handler
	{
		virtual void handle_timeout(Duration curr_time) = 0;
	};

	/**
	 * Return the current time of the source
	 */
	virtual Duration curr_time() = 0;

	/**
	 * Return the maximum timeout duration that the source can handle
	 */
	virtual Microseconds max_timeout() const = 0;

	/**
	 * Install a timeout, overrides the last timeout if any
	 *
	 * \param duration  timeout duration
	 * \param handler   timeout callback
	 */
	virtual void schedule_timeout(Microseconds     duration,
	                              Timeout_handler &handler) = 0;

	/**
	 * Tell the time source which scheduler to use for its own timeouts
	 *
	 * This method enables a time source for example to synchronize with an
	 * accurate but expensive timer only on a periodic basis while using a
	 * cheaper interpolation in general.
	 */
	virtual void scheduler(Timeout_scheduler &scheduler) { };
};

#endif /* _OS__TIME_SOURCE_H_ */
