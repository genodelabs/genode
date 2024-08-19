/*
 * \brief  Tiled-WM test: example application
 * \author Christian Helmuth
 * \date   2018-09-26
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_rom_dataspace.h>

/* local includes */
#include <util.h>
#include "app.h"


struct Main
{
	Libc::Env &env;

	Genode::Attached_rom_dataspace config { env, "config" };

	QApplication &app { qt6_initialization(env) };

	QMember<App> widget { name_from_config() };

	QString name_from_config()
	{
		Name name = config.xml().attribute_value("name", Name("no name"));

		return QString(name.string());
	}

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

