/*
 * \brief  Access to the core's log facility
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \date   2016-05-03
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>

/* base-internal includes */
#include <base/internal/globals.h>
#include <base/internal/output.h>
#include <base/internal/unmanaged_singleton.h>

/* core includes */
#include <core_log.h>


Genode::Log &Genode::Log::log()
{
	struct Buffer
	{
		struct Write_fn : Core_log
		{
			void operator () (char const *s) { output(s); }
		} function { };

		Buffered_output<512, Write_fn> buffer { function };
		Log                            log    { buffer   };
	};

	return unmanaged_singleton<Buffer>()->log;
}


void Genode::init_log() { };
