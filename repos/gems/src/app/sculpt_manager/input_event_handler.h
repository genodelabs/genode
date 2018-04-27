/*
 * \brief  Interface for handling input events
 * \author Norman Feske
 * \date   2018-05-02
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INPUT_EVENT_HANDLER_H_
#define _INPUT_EVENT_HANDLER_H_

/* Genode includes */
#include <input/event.h>

/* local includes */
#include "types.h"

namespace Sculpt { struct Input_event_handler; }

struct Sculpt::Input_event_handler : Interface
{
	virtual void handle_input_event(Input::Event const &) = 0;
};

#endif /* _INPUT_EVENT_HANDLER_H_ */
