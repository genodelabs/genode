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

#include <legacy/lx_kit/env.h>

static Lx_kit::Env *_env_ptr;


Lx_kit::Env &Lx_kit::env()
{
	return *_env_ptr;
}


Lx_kit::Env &Lx_kit::construct_env(Genode::Env &env)
{
	static Lx_kit::Env _env(env);
	_env_ptr = &_env;
	return _env;
}
