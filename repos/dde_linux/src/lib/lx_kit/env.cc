/*
 * \brief  Lx_kit environment
 * \author Stefan Kalkowski
 * \date   2021-03-16
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_kit/env.h>


Lx_kit::Env & Lx_kit::env(Genode::Env * env)
{
	static Lx_kit::Env environment(*env);
	return environment;
}
