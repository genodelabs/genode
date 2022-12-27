/*
 * \brief  GUI wrapper for monitoring the user input of GUI components
 * \author Norman Feske
 * \date   2018-04-30
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _NITPICKER_H_
#define _NITPICKER_H_

/* Genode includes */
#include <root/component.h>
#include <gui_session/gui_session.h>
#include <base/component.h>

/* local includes */
#include "input_event_handler.h"

namespace Gui {

	using namespace Genode;
	using Sculpt::Input_event_handler;

	struct Session_component;
	struct Root;
}


struct Gui::Root : Genode::Root_component<Session_component>
{
	Env &_env;
	Input_event_handler &_event_handler;
	Input::Seq_number   &_global_input_seq_number;

	Session_component *_create_session(const char *) override;
	void               _upgrade_session(Session_component *, const char *) override;
	void               _destroy_session(Session_component *) override;

	Root(Env &, Allocator &, Input_event_handler &, Input::Seq_number &);

	~Root();
};

#endif /* _NITPICKER_H_ */
