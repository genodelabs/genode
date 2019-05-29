/*
 * \brief  Component providing a Terminal session via SSH
 * \author Josef Soentgen
 * \date   2018-09-25
 *
 * On the local side this component provides Terminal sessions to its
 * configured clients while it also provides SSH sessions on the remote side.
 * The relation between both sides is establish via the policy settings that
 * determine which Terminal session may be accessed by a SSH login and the
 * other way around.
 *
 * When the component gets started it will create a read-only login database.
 * A login consists of a username and either a password or public-key or both.
 * The username is the unique primary key and is used to identify the right
 * Terminal session when a login is attempted. In return, it is also used to
 * attach a Terminal session to an (existing) SSH session. The SSH protocol
 * processing is done via libssh running in its own event thread while the
 * EP handles the Terminal session. Locking is done at the appropriate places
 * to synchronize both threads.
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_ram_dataspace.h>
#include <base/attached_rom_dataspace.h>

/* local includes */
#include "root_component.h"


struct Main
{
	Genode::Env &_env;

	Genode::Sliced_heap      _sliced_heap { _env.ram(), _env.rm() };
	Terminal::Root_component _root        { _env, _sliced_heap };

	Main(Genode::Env &env) : _env(env)
	{
		Genode::log("--- SSH terminal started ---");
		_env.parent().announce(env.ep().manage(_root));
	}
};


void Libc::Component::construct(Libc::Env &env) { static Main main(env); }
