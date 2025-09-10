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
#include <cpu_thread/client.h>

/* base-internal includes */
#include <base/internal/runtime.h>

using namespace Genode;


void Thread::_init_trace_control()
{
	Dataspace_capability ds = _runtime.cpu.trace_control();
	if (ds.valid()) {
		Region_map::Attr attr { };
		attr.writeable = true;
		_runtime.local_rm.attach(ds, attr).with_result(
			[&] (Local_rm::Attachment &a) {
				a.deallocate = false;
				_trace_control = reinterpret_cast<Trace::Control *>(a.ptr); },
			[&] (Region_map::Attach_error) {
				error("failed to initialize trace control for new thread"); }
		);
	}
}


void Thread::_deinit_trace_control()
{
	if (_trace_control)
		_runtime.local_rm.detach(addr_t(_trace_control));
}
