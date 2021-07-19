/*
 * \brief  Lx_kit backend for Linux kernel initialization
 * \author Stefan Kalkowski
 * \date   2021-03-10
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _LX_KIT__INIT_H_
#define _LX_KIT__INIT_H_

#include <base/env.h>
#include <base/heap.h>

namespace Lx_kit {
	using namespace Genode;

	void initialize(Env & env);
	class Initcalls;
}


class Lx_kit::Initcalls
{
	private:

		struct E : List<E>::Element
		{
			unsigned int prio;
			int (* call) (void);

			E(unsigned int p, int (*fn)(void)) : prio(p), call(fn) {}
		};

		Heap  & _heap;
		List<E> _call_list {};

	public:

		void add(int (*initcall)(void), unsigned int prio);
		void execute_in_order();

		Initcalls(Heap & heap) : _heap(heap) {}
};

#endif /* _LX_KIT__INIT_H_ */
