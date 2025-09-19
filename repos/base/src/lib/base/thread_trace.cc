/*
 * \brief  Implementation of the Thread API
 * \author Norman Feske
 * \date   2025-09-12
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/thread.h>

/* base-internal includes */
#include <base/internal/runtime.h>

using namespace Genode;


void Thread::_init_trace_control()
{
	_runtime.trace_control.with_result(
		[&] (Local::Constrained_region_map::Attachment const &a) {
			_trace_control = reinterpret_cast<Trace::Control *>(a.ptr); },
		[] (auto) { });
}
