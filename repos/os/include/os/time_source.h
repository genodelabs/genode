/*
 * \brief  Interface of a time source that can handle one timeout at a time
 * \author Martin Stein
 * \date   2016-11-04
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _OS__TIME_SOURCE_H_
#define _OS__TIME_SOURCE_H_

namespace Genode { class Time_source; }

/**
 * Interface of a time source that can handle one timeout at a time
 */
struct Genode::Time_source
{
	/**
	 * Makes it clear which time unit an interfaces takes
	 */
	struct Microseconds
	{
		unsigned long value;

		explicit Microseconds(unsigned long const value) : value(value) { }
	};

	/**
	 * Interface of a timeout callback
	 */
	struct Timeout_handler
	{
		virtual void handle_timeout(Microseconds curr_time) = 0;
	};

	/**
	 * Return the current time of the source
	 */
	virtual Microseconds curr_time() const = 0;

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
};

#endif /* _OS__TIME_SOURCE_H_ */
