/*
 * \brief  Genode environment for ACPICA library
 * \author Christian Helmuth
 * \date   2016-11-14
 */

/*
 * Copyright (C) 2016-2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <util/reconstructible.h>
#include <acpica/acpica.h>
#include <platform_session/connection.h>

#include "env.h"


namespace Acpica { struct Env; }


struct Acpica::Env
{
	Genode::Env       &env;
	Genode::Allocator &heap;

	Genode::Constructible<Platform::Connection> platform;

	Env(Genode::Env &env, Genode::Allocator &heap) : env(env), heap(heap) { }
};

static Genode::Constructible<Acpica::Env> instance;


Genode::Allocator & Acpica::heap()     { return instance->heap; }
Genode::Env       & Acpica::env()      { return instance->env; }
Platform::Client  & Acpica::platform()
{
	if (!instance->platform.constructed())
		instance->platform.construct(instance->env);

	return *instance->platform;
}


void Acpica::init(Genode::Env &env, Genode::Allocator &heap)
{
	instance.construct(env, heap);

	platform();
}
