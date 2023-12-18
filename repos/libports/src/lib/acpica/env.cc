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

#include "env.h"


namespace Acpica { struct Env; }


struct Acpica::Env
{
	Genode::Env       &env;
	Genode::Allocator &heap;

	Env(Genode::Env &env, Genode::Allocator &heap) : env(env), heap(heap) { }
};

static Acpica::Env * instance;


Genode::Allocator & Acpica::heap()     { return instance->heap; }
Genode::Env       & Acpica::env()      { return instance->env; }


void Acpica::init(Genode::Env &env, Genode::Allocator &heap)
{
	static Acpica::Env _instance { env, heap };

	instance = &_instance;
}
