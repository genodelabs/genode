/*
 * \brief  Interface for unique serial numbers
 * \author Christian Helmuth
 * \date   2025-12-12
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _EVENT_FILTER__SERIAL_H_
#define _EVENT_FILTER__SERIAL_H_

/* Genode includes */
#include <os/reporter.h>

namespace Event_filter { struct Serial; }

struct Event_filter::Serial : Interface
{
	virtual unsigned number() = 0;
};

#endif /* _EVENT_FILTER__SERIAL_H_ */
