/*
 * \brief  Tiled-WM test: panel
 * \author Christian Helmuth
 * \date   2018-09-26
 *
 * Panel is a Qt6-based example panel at the bottom of the screen.
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include "panel.h"


struct Main
{
	Libc::Env &env;

	Genode::Entrypoint signal_ep { env, 0x4000, "signal ep",
	                               Genode::Affinity::Location() };

	QApplication &app { qt6_initialization(env) };

	QMember<Panel> widget { env, signal_ep };

	Main(Libc::Env &env) : env(env)
	{
		widget->show();
	}
};


void Libc::Component::construct(Libc::Env &env)
{
	Libc::with_libc([&] {

		static Main main { env };

		exit(main.app.exec());
	});
}
