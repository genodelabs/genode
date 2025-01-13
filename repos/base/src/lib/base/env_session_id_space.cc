/*
 * \brief  Component-local session ID space
 * \author Norman Feske
 * \date   2016-10-13
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/connection.h>
#include <base/service.h>

/* base-internal includes */
#include <base/internal/globals.h>

using namespace Genode;


Id_space<Parent::Client> &Genode::env_session_id_space()
{
	static Id_space<Parent::Client> id_space { };

	/* pre-allocate env session IDs */
	static Parent::Client dummy;
	static Id_space<Parent::Client>::Element
		pd     { dummy, id_space, Parent::Env::pd()     },
		cpu    { dummy, id_space, Parent::Env::cpu()    },
		log    { dummy, id_space, Parent::Env::log()    },
		binary { dummy, id_space, Parent::Env::binary() },
		linker { dummy, id_space, Parent::Env::linker() };

	return id_space;
}
