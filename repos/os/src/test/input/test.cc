/*
 * \brief  Input service test program
 * \author Christian Helmuth
 * \date   2010-06-15
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/env.h>
#include <base/log.h>
#include <timer_session/connection.h>
#include <input_session/connection.h>
#include <input/event.h>


using namespace Genode;

static char const * ev_type(Input::Event::Type type)
{
	switch (type) {
	case Input::Event::INVALID: return "INVALID";
	case Input::Event::MOTION:  return "MOTION ";
	case Input::Event::PRESS:   return "PRESS  ";
	case Input::Event::RELEASE: return "RELEASE";
	case Input::Event::WHEEL:   return "WHEEL  ";
	case Input::Event::FOCUS:   return "FOCUS  ";
	case Input::Event::LEAVE:   return "LEAVE  ";
	case Input::Event::TOUCH:   return "TOUCH  ";
	}

	return "";
}


static char const * key_name(Input::Event *ev)
{
	if (ev->type() == Input::Event::MOTION)
		return "";

	return Input::key_name(static_cast<Input::Keycode>(ev->code()));
}


int main(int argc, char **argv)
{
	/*
	 * Init sessions to the required external services
	 */
	static Input::Connection input;
	static Timer::Connection timer;

	log("--- Input test is up ---");

	Input::Event *ev_buf = (env()->rm_session()->attach(input.dataspace()));

	log("input buffer at ", ev_buf);

	/*
	 * Handle input events
	 */
	int key_cnt = 0;
	while (1) {
		/* poll input service every 20 ms */
		while (!input.pending()) timer.msleep(20);

		for (int i = 0, num_ev = input.flush(); i < num_ev; ++i) {

			Input::Event *ev = &ev_buf[i];

			if (ev->type() == Input::Event::PRESS)   key_cnt++;
			if (ev->type() == Input::Event::RELEASE) key_cnt--;

			/* log event */
			log("Input event "
			    "type=",    ev_type(ev->type()),   "\t"
			    "code=",    ev->code(), "\t"
			    "rx=",      ev->rx(),   "\t"
			    "ry=",      ev->ry(),   "\t"
			    "ax=",      ev->ax(),   "\t"
			    "ay=",      ev->ay(),   "\t"
			    "key_cnt=", key_cnt,    "\t", key_name(ev));
		}
	}

	return 0;
}
