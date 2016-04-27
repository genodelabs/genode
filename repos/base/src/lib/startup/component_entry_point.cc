/*
 * \brief   Component entry point for dynamic executables
 * \author  Christian Helmuth
 * \date    2016-01-21
 *
 * The ELF entry point of dynamic binaries is set to component_entry_point(),
 * which calls the call_component_construct-hook function pointer.
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/component.h>


/* FIXME move to base-internal header */
namespace Genode {
	extern void (*call_component_construct)(Env &);
}

namespace Genode {

	void component_entry_point(Genode::Env &env)
	{
		call_component_construct(env);
	}
}
