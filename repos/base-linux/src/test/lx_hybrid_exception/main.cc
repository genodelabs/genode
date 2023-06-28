/*
 * \brief  Test if the exception mechanism works in hybrid applications
 * \author Christian Prochaska
 * \date   2011-11-22
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/log.h>

/* Linux includes */
#include <stdlib.h>

using namespace Genode;


class Test_exception { };


void Component::construct(Genode::Env &env)
{
	log("--- lx_hybrid exception test ---");

	try {
		log("Throwing Test_exception");
		throw Test_exception();
	} catch (Test_exception) {
		log("Caught Test_exception");
	}

	log("--- returning from main ---");
	env.parent().exit(0);
}
