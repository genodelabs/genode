/*
 * \brief  Access to the component's LOG session
 * \author Norman Feske
 * \date   2016-05-03
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>

using namespace Genode;


void Log::_acquire(Type type)
{
	_mutex.acquire();

	/*
	 * Mark warnings and errors via distinct colors.
	 */
	switch (type) {
	case LOG:                                              break;
	case WARNING: _output.out_string("\033[34mWarning: "); break;
	case ERROR:   _output.out_string("\033[31mError: ");   break;
	};
}


void Log::_release()
{
	/*
	 * Reset color and add newline
	 */
	_output.out_string("\033[0m\n");

	_mutex.release();
}


void Raw::_acquire()
{
	/*
	 * Mark raw output with distinct color
	 */
	_output().out_string("\033[32mKernel: ");
}


void Raw::_release()
{
	/*
	 * Reset color and add newline
	 */
	_output().out_string("\033[0m\n");
}


void Trace_output::_acquire()
{
	_mutex.acquire();
}


void Trace_output::_release()
{
	/*
	 * Add newline
	 */
	_output.out_string("\n");

	_mutex.release();
}
