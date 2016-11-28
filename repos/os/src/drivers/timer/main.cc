/*
 * \brief  Provides the Timer service to multiple clients
 * \author Norman Feske
 * \author Martin Stein
 * \date   2006-08-15
 */

/*
 * Copyright (C) 2006-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/heap.h>
#include <base/component.h>

/* local includes */
#include <root_component.h>

using namespace Genode;


class Main
{
	private:

		Sliced_heap           _sliced_heap;
		Timer::Root_component _root;

	public:

		Main(Env &env) : _sliced_heap(env.ram(), env.rm()),
		                 _root(env.ep(), _sliced_heap)
		{
			env.parent().announce(env.ep().manage(_root));
		}
};


size_t Component::stack_size()      { return 4*1024*sizeof(addr_t); }
void Component::construct(Env &env) { static Main main(env); }
