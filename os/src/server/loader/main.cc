/*
 * \brief  Loader
 * \author Christian Prochaska
 * \date   2010-09-16
 */

/*
 * Copyright (C) 2010-2011 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/sleep.h>

/* local includes */
#include <loader_root.h>

using namespace Genode;

int main()
{
	enum { STACK_SIZE = 8*1024 };
	static Cap_connection cap;
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "loader_ep");

	static Loader::Root loader_root(&ep, env()->heap());
	env()->parent()->announce(ep.manage(&loader_root));

	sleep_forever();
	return 0;
}
