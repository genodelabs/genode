/*
 * \brief  Tiled-WM test: panel
 * \author Christian Helmuth
 * \date   2018-09-26
 *
 * Panel is a Qt5-based example panel at the bottom of the screen.
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

	Genode_signal_dispatcher dispatcher { env };

	QApplication &app { qt5_initialization(env) };

	QMember<Panel> widget { env, dispatcher.signal_receiver() };

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
