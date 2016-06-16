/*
 * \brief  Access to the component's LOG session
 * \author Norman Feske
 * \date   2016-05-03
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/log.h>

using namespace Genode;


void Log::_acquire(Type type)
{
	_lock.lock();

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

	_lock.unlock();
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
