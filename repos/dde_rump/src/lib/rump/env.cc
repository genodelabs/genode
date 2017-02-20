/**
 * \brief  Rump::Env initialization
 * \author Sebastian Sumpf
 * \date   2016-06-23
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <rump/env.h>

/*
 * Genode nviroment instance
 */
static Genode::Constructible<Rump::Env> _env;


Rump::Env &Rump::env()
{
	return *_env;
}


void Rump::construct_env(Genode::Env &env)
{
	_env.construct(env);
}
