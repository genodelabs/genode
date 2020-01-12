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
#include <util/interface.h>
#include <input/event.h>

namespace Nitpicker { struct Input_event_handler; }

struct Nitpicker::Input_event_handler : Genode::Interface
{
	virtual void handle_input_event(Input::Event const &) = 0;
};

#endif /* _INPUT_EVENT_HANDLER_H_ */
