/*
 * \brief  Test for the use of 'Shared_object' API
 * \author Sebastian Sumpf
 * \author Norman Feske
 * \date   2014-05-20
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>

/* test-local includes */
#include "test-ldso.h"

using namespace Genode;

struct Global_object
{
	Global_object()
	{
		Genode::log("Global object constructed");
	}
};

/*
 * The 'global_object' is expected to be construced at the load time of the
 * shared object.
 */
Global_object global_object;


/*
 * XXX currently untested
 */
extern "C" void lib_dl_symbol()
{
	Genode::log("called (from '", __func__, "')");
	Genode::log("Call 'lib_1_good': ");
	lib_1_good();
}
