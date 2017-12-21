/*
 * \brief  Test for changing configuration at runtime (server-side)
 * \author Norman Feske
 * \author Martin Stein
 * \date   2012-04-04
 *
 * This program provides a generated config file as ROM service. After
 * opening a ROM session, the data gets repeatedly updated.
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_ram_dataspace.h>
#include <os/static_root.h>
#include <timer_session/connection.h>
#include <rom_session/rom_session.h>
#include <base/component.h>

using namespace Genode;

/*
 * The implementation of this class follows the lines of
 * 'os/include/os/child_policy_dynamic_rom.h'.
 */
class Rom_session_component : public Rpc_object<Rom_session>
{
	private:

		Env                       &_env;
		Attached_ram_dataspace     _fg              { _env.ram(), _env.rm(), 0 };
		Attached_ram_dataspace     _bg              { _env.ram(), _env.rm(), 0 };
		bool                       _bg_pending_data { false };
		Signal_context_capability  _sigh            { };

	public:

		Rom_session_component(Env &env) : _env(env) { }

		/**
		 * Update the config file
		 */
		void configure(char const *data)
		{
			size_t const data_len = strlen(data) + 1;

			/* let background buffer grow if needed */
			if (_bg.size() < data_len)
				_bg.realloc(&_env.ram(), data_len);

			strncpy(_bg.local_addr<char>(), data, data_len);
			_bg_pending_data = true;

			/* inform client about the changed data */
			if (_sigh.valid())
				Signal_transmitter(_sigh).submit();
		}


		/***************************
		 ** ROM session interface **
		 ***************************/

		Rom_dataspace_capability dataspace() override
		{
			if (!_fg.size() && !_bg_pending_data) {
				error("no data loaded");
				return Rom_dataspace_capability();
			}
			/*
			 * Keep foreground if no background exists. Otherwise, use old
			 * background as new foreground.
			 */
			if (_bg_pending_data) {
				_fg.swap(_bg);
				_bg_pending_data = false;
			}
			Dataspace_capability ds_cap = _fg.cap();
			return static_cap_cast<Rom_dataspace>(ds_cap);
		}

		void sigh(Signal_context_capability sigh_cap) override { _sigh = sigh_cap; }
};

struct Main
{
	enum { STACK_SIZE = 2 * 1024 * sizeof(addr_t) };

	Env                      &env;
	Rom_session_component     rom_session   { env };
	Static_root<Rom_session>  rom_root      { env.ep().manage(rom_session) };
	int                       counter       { -1 };
	Timer::Connection         timer         { env };
	Signal_handler<Main>      timer_handler { env.ep(), *this, &Main::handle_timer };

	void handle_timer()
	{
		String<100> config("<config><counter>", counter++, "</counter></config>");
		rom_session.configure(config.string());
		timer.trigger_once(250 * 1000);
	}

	Main(Env &env) : env(env)
	{
		timer.sigh(timer_handler);
		handle_timer();
		env.parent().announce(env.ep().manage(rom_root));
	}
};

void Component::construct(Env &env) { static Main main(env); }
