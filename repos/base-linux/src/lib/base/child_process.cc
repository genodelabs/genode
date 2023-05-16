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
	_cpu(cpu),
	_cap(cpu.create_thread(pd, name, Affinity::Location(), Cpu_session::Weight()))
{ }


Child::Initial_thread::~Initial_thread() { }


void Child::Initial_thread::start(addr_t, Start &) { }


/*
 * On Linux, the ELF loading is performed by the kernel
 */
Child::Process::Loaded_executable::Loaded_executable(Type,
                                                     Dataspace_capability,
                                                     Ram_allocator &,
                                                     Region_map &,
                                                     Region_map &,
                                                     Parent_capability) { }


Child::Process::Process(Type                   type,
                        Dataspace_capability   ldso_ds,
                        Pd_session            &pd,
                        Initial_thread_base   &,
                        Initial_thread::Start &,
                        Region_map            &local_rm,
                        Region_map            &remote_rm,
                        Parent_capability      parent_cap)
:
	loaded_executable(type, ldso_ds, pd, local_rm, remote_rm, parent_cap)
{
	/* skip loading when called during fork */
	if (type == TYPE_FORKED)
		return;

	/*
	 * If the specified executable is a dynamically linked program, we load
	 * the dynamic linker instead.
	 */
	if (!ldso_ds.valid()) {
		error("attempt to start dynamic executable without dynamic linker");
		throw Missing_dynamic_linker();
	}

	pd.assign_parent(parent_cap);

	Linux_native_pd_client lx_pd(pd.native_pd());

	lx_pd.start(ldso_ds);
}


Child::Process::~Process() { }
