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


struct Main
{
	Env                  &_env;
	Input::Connection     _input      { _env };
	Signal_handler<Main>  _input_sigh { _env.ep(), *this, &Main::_handle_input };
	unsigned              _event_cnt  { 0 };
	int                   _key_cnt    { 0 };

	void _handle_input()
	{
		_input.for_each_event([&] (Input::Event const &ev) {
			if (ev.press())   ++_key_cnt;
			if (ev.release()) --_key_cnt;
			log("Input event #", _event_cnt++, "\t", ev, "\tkey count: ", _key_cnt);
		});
	}

	Main(Env &env) : _env(env)
	{
		log("--- Input test ---");
		_input.sigh(_input_sigh);
	}
};


void Component::construct(Env &env) { static Main main(env); }
