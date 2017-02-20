/*
 * \brief  A server for connecting two 'Terminal' sessions
 * \author Christian Prochaska
 * \date   2012-05-16
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/heap.h>

/* local includes */
#include "terminal_root.h"

namespace Terminal_crosslink {

	using namespace Genode;

	struct Main;
}

struct Terminal_crosslink::Main
{
	Env  &_env;
	Heap  _heap { _env.ram(), _env.rm() };

	Root  _terminal_root { _env, _heap };

	Main(Env &env) : _env(env)
	{
		env.parent().announce(env.ep().manage(_terminal_root));
	}
};

void Component::construct(Genode::Env &env)
{
	static Terminal_crosslink::Main main(env);
}
