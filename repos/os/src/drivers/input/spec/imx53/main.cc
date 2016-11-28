/**
 * \brief  Input driver front-end
 * \author Norman Feske
 * \author Christian Helmuth
 * \author Stefan Kalkowski
 * \date   2006-08-30
 */

/*
 * Copyright (C) 2006-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/env.h>
#include <base/sleep.h>
#include <base/rpc_server.h>
#include <cap_session/connection.h>
#include <platform_session/connection.h>
#include <input/component.h>
#include <input/root.h>
#include <base/printf.h>
#include <os/server.h>

/* local includes */
#include <driver.h>

using namespace Genode;


struct Main
{
	Server::Entrypoint &ep;

	Input::Session_component session;
	Input::Root_component    root;

	Main(Server::Entrypoint &ep)
	: ep(ep), root(ep.rpc_ep(), session)
	{
		Platform::Connection plat_drv;
		switch (plat_drv.revision()) {
			case Platform::Session::SMD:
				plat_drv.enable(Platform::Session::I2C_2);
				plat_drv.enable(Platform::Session::I2C_3);
				plat_drv.enable(Platform::Session::BUTTONS);
				Input::Tablet_driver::factory(ep, session.event_queue());
				break;
			default:
				PWRN("No input driver available for this board");
		}

		/* tell parent about the service */
		env()->parent()->announce(ep.manage(root));
	}
};

/************
 ** Server **
 ************/

namespace Server {
	char const *name()             { return "input_drv_ep";       }
	size_t stack_size()            { return 16*1024*sizeof(long); }
	void construct(Entrypoint &ep) { static Main server(ep);      }
}
