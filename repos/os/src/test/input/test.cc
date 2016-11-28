/*
 * \brief  Input service test program
 * \author Christian Helmuth
 * \date   2010-06-15
 */

/*
 * Copyright (C) 2010-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/component.h>
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


static char const * key_name(Input::Event const &ev)
{
	if (ev.type() == Input::Event::MOTION)
		return "";

	return Input::key_name(static_cast<Input::Keycode>(ev.code()));
}


class Test_environment
{
	private:

		Genode::Env &_env;

		Input::Connection _input;

		Genode::Signal_handler<Test_environment> _input_sigh;

		unsigned int event_count = 0;

		void _handle_input();

	public:

		Test_environment(Genode::Env &env)
		: _env(env),
		  _input_sigh(env.ep(), *this, &Test_environment::_handle_input)
		{
			log("--- Input test is up ---");

			_input.sigh(_input_sigh);
		}
};


void Test_environment::_handle_input()
{
	/*
	 * Handle input events
	 */
	int key_cnt = 0;

	_input.for_each_event([&] (Input::Event const &ev) {
		event_count++;

		if (ev.type() == Input::Event::PRESS)   key_cnt++;
		if (ev.type() == Input::Event::RELEASE) key_cnt--;

		/* log event */
		log("Input event #", event_count, "\t"
		    "type=",         ev_type(ev.type()), "\t"
		    "code=",         ev.code(),          "\t"
		    "rx=",           ev.rx(),            "\t"
		    "ry=",           ev.ry(),            "\t"
		    "ax=",           ev.ax(),            "\t"
		    "ay=",           ev.ay(),            "\t"
		    "key_cnt=",      key_cnt, "\t", key_name(ev));
	});
}

void Component::construct(Genode::Env &env)
{
	using namespace Genode;

	log("--- Test input ---\n");
	static Test_environment te(env);
}
