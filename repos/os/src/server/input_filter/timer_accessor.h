/*
 * \brief  Interface for accessing a timer
 * \author Norman Feske
 * \date   2017-02-01
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INPUT_FILTER__TIMER_ACCESSOR_H_
#define _INPUT_FILTER__TIMER_ACCESSOR_H_

/* Genode includes */
#include <timer_session/connection.h>

namespace Input_filter { struct Timer_accessor; }

struct Input_filter::Timer_accessor : Interface { virtual Timer::Connection &timer() = 0; };

#endif /* _INPUT_FILTER__TIMER_ACCESSOR_H_ */
