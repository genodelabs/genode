/*
 * \brief  Input service test program
 * \author Christian Helmuth
 * \date   2010-06-15
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <timer_session/connection.h>
#include <input_session/connection.h>

using namespace Genode;


static char const * ev_type(Input::Event::Type type)
{
	switch (type) {
	case Input::Event::INVALID:   return "INVALID";
	case Input::Event::MOTION:    return "MOTION ";
	case Input::Event::PRESS:     return "PRESS  ";
	case Input::Event::RELEASE:   return "RELEASE";
	case Input::Event::WHEEL:     return "WHEEL  ";
	case Input::Event::FOCUS:     return "FOCUS  ";
	case Input::Event::LEAVE:     return "LEAVE  ";
	case Input::Event::TOUCH:     return "TOUCH  ";
	case Input::Event::CHARACTER: return "CHARACTER";
	}

	return "";
}


static char const * key_name(Input::Event const &ev)
{
	if (ev.type() == Input::Event::MOTION)
		return "";

	return Input::key_name(static_cast<Input::Keycode>(ev.code()));
}


struct Main
{
	Env                  &env;
	Input::Connection     input      { env };
	Signal_handler<Main>  input_sigh { env.ep(), *this, &Main::handle_input };
	unsigned              event_cnt  { 0 };

	void handle_input();

	Main(Env &env) : env(env)
	{
		log("--- Input test ---");
		input.sigh(input_sigh);
	}
};


void Main::handle_input()
{
	int key_cnt = 0;
	input.for_each_event([&] (Input::Event const &ev) {
		event_cnt++;

		if (ev.type() == Input::Event::PRESS)   key_cnt++;
		if (ev.type() == Input::Event::RELEASE) key_cnt--;

		log("Input event #", event_cnt,          "\t"
		    "type=",         ev_type(ev.type()), "\t"
		    "code=",         ev.code(),          "\t"
		    "rx=",           ev.rx(),            "\t"
		    "ry=",           ev.ry(),            "\t"
		    "ax=",           ev.ax(),            "\t"
		    "ay=",           ev.ay(),            "\t"
		    "key_cnt=",      key_cnt,            "\t", key_name(ev));
	});
}


void Component::construct(Env &env) { static Main main(env); }
