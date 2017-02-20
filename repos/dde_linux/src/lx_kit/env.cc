/*
 * \brief  Usb::Env initialization
 * \author Sebastian Sumpf
 * \date   2016-06-23
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_kit/env.h>

/*
 * Lx_kit enviroment instance
 */
static Genode::Constructible<Lx_kit::Env> _env;


Lx_kit::Env &Lx_kit::env()
{
	return *_env;
}


Lx_kit::Env &Lx_kit::construct_env(Genode::Env &env)
{
	_env.construct(env);
	return *_env;
}
