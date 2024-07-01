/*
 * \brief  Implementation of process creation for Linux
 * \author Norman Feske
 * \date   2006-07-06
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/env.h>
#include <base/child.h>
#include <base/log.h>
#include <linux_native_pd/client.h>

/* base-internal includes */
#include <linux_syscalls.h>
#include <base/internal/elf.h>

using namespace Genode;


static Thread_capability create_thread(auto &pd, auto &cpu, auto const &name)
{
	return cpu.create_thread(pd, name, { }, { }).template convert<Thread_capability>(
		[&] (Thread_capability cap) { return cap; },
		[&] (Cpu_session::Create_thread_error) {
			error("failed to create thread via CPU session");
			return Thread_capability(); });
}


/*
 * Register main thread at core
 *
 * At this point in time, we do not yet know the TID and PID of the new
 * thread. Those information will be provided to core by the constructor of
 * the 'Platform' of the new process.
 */
Child::Initial_thread::Initial_thread(Cpu_session          &cpu,
                                      Pd_session_capability pd,
                                      Name           const &name)
:
	_cpu(cpu), _cap(create_thread(pd, cpu, name))
{ }


Child::Initial_thread::~Initial_thread() { }


void Child::Initial_thread::start(addr_t, Start &) { }


Child::Start_result Child::_start_process(Dataspace_capability   ldso_ds,
                                          Pd_session            &pd,
                                          Initial_thread_base   &,
                                          Initial_thread::Start &,
                                          Region_map            &,
                                          Region_map            &,
                                          Parent_capability)
{
	Linux_native_pd_client(pd.native_pd()).start(ldso_ds);
	return Start_result::OK;
}
