/*
 * \brief  Linux-specific platform thread implementation
 * \author Norman Feske
 * \date   2007-10-15
 */

/*
 * Copyright (C) 2007-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <util/token.h>
#include <util/misc_math.h>

/* core includes */
#include <platform_thread.h>
#include <linux_syscalls.h>

using namespace Core;


void Platform_thread::submit_exception(unsigned pid)
{
	bool submitted = false;
	_registry().for_each([&] (Platform_thread const &thread) {
		if (submitted || thread._tid != pid)
			return;

		Signal_context_capability sigh = thread._pager._sigh;
		if (sigh.valid())
			Signal_transmitter(sigh).submit();

		submitted = true;
	});
}


Registry<Platform_thread> &Platform_thread::_registry()
{
	static Registry<Platform_thread> registry { };
	return registry;
}
