/*
 * \brief  Tiled-WM test: example overlay
 * \author Christian Helmuth
 * \date   2018-09-28
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include <util.h>
#include "overlay.h"


struct Main
{
	Libc::Env &env;

	QApplication &app { qt6_initialization(env) };

	QMember<Overlay> widget { };

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

