/*
 * \brief  Test for changing configuration at runtime (server-side)
 * \author Norman Feske
 * \date   2012-04-04
 *
 * This program provides a generated config file as ROM service. After
 * opening a ROM session, the data gets repeatedly updated.
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/signal.h>
#include <os/attached_ram_dataspace.h>
#include <os/static_root.h>
#include <timer_session/connection.h>
#include <rom_session/rom_session.h>
#include <cap_session/connection.h>


/*
 * The implementation of this class follows the lines of
 * 'os/include/os/child_policy_dynamic_rom.h'.
 */
class Rom_session_component : public Genode::Rpc_object<Genode::Rom_session>
{
	private:

		Genode::Attached_ram_dataspace _fg;
		Genode::Attached_ram_dataspace _bg;

		bool _bg_has_pending_data;

		Genode::Lock _lock;

		Genode::Signal_context_capability _sigh;

	public:

		/**
		 * Constructor
		 */
		Rom_session_component()
		: _fg(0, 0), _bg(0, 0), _bg_has_pending_data(false) { }

		/**
		 * Update the config file
		 */
		void configure(char const *data)
		{
			Genode::Lock::Guard guard(_lock);

			Genode::size_t const data_len = Genode::strlen(data) + 1;

			/* let background buffer grow if needed */
			if (_bg.size() < data_len)
				_bg.realloc(Genode::env()->ram_session(), data_len);

			Genode::strncpy(_bg.local_addr<char>(), data, data_len);
			_bg_has_pending_data = true;

			/* inform client about the changed data */
			if (_sigh.valid())
				Genode::Signal_transmitter(_sigh).submit();
		}


		/***************************
		 ** ROM session interface **
		 ***************************/

		Genode::Rom_dataspace_capability dataspace()
		{
			Genode::Lock::Guard guard(_lock);

			if (!_fg.size() && !_bg_has_pending_data) {
				PERR("Error: no data loaded");
				return Genode::Rom_dataspace_capability();
			}

			/*
			 * Keep foreground if no background exists. Otherwise, use old
			 * background as new foreground.
			 */
			if (_bg_has_pending_data) {
				_fg.swap(_bg);
				_bg_has_pending_data = false;
			}

			Genode::Dataspace_capability ds_cap = _fg.cap();
			return Genode::static_cap_cast<Genode::Rom_dataspace>(ds_cap);
		}

		void sigh(Genode::Signal_context_capability sigh_cap)
		{
			Genode::Lock::Guard guard(_lock);
			_sigh = sigh_cap;
		}
};


int main(int argc, char **argv)
{
	using namespace Genode;

	/* connection to capability service needed to create capabilities */
	static Cap_connection cap;

	enum { STACK_SIZE = 8*1024 };
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "rom_ep");

	static Rom_session_component rom_session;
	static Static_root<Rom_session> rom_root(ep.manage(&rom_session));

	rom_session.configure("<config><counter>-1</counter></config>");

	/* announce server */
	env()->parent()->announce(ep.manage(&rom_root));

	int counter = 0;
	for (;;) {

		static Timer::Connection timer;
		timer.msleep(250);

		/* re-generate configuration */
		char buf[100];
		Genode::snprintf(buf, sizeof(buf),
		                 "<config><counter>%d</counter></config>",
		                 counter++);

		rom_session.configure(buf);
	}
	return 0;
};
