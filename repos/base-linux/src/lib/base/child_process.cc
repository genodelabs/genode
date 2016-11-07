/*
 * \brief  Implementation of process creation for Linux
 * \author Norman Feske
 * \date   2006-07-06
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
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
 * the 'Platform_env' of the new process.
 */
Child::Initial_thread::Initial_thread(Cpu_session          &cpu,
                                      Pd_session_capability pd,
                                      Name           const &name)
:
	_cpu(cpu),
	_cap(cpu.create_thread(pd, name, Affinity::Location(), Cpu_session::Weight()))
{ }


Child::Initial_thread::~Initial_thread() { }


void Child::Initial_thread::start(addr_t) { }


/*
 * On Linux, the ELF loading is performed by the kernel
 */
Child::Process::Loaded_executable::Loaded_executable(Dataspace_capability,
                                                     Dataspace_capability,
                                                     Ram_session &,
                                                     Region_map &,
                                                     Region_map &,
                                                     Parent_capability) { }


Child::Process::Process(Dataspace_capability  elf_ds,
                        Dataspace_capability  ldso_ds,
                        Pd_session_capability pd_cap,
                        Pd_session           &pd,
                        Ram_session          &ram,
                        Initial_thread_base  &initial_thread,
                        Region_map           &local_rm,
                        Region_map           &remote_rm,
                        Parent_capability     parent_cap)
:
	initial_thread(initial_thread),
	loaded_executable(elf_ds, ldso_ds, ram, local_rm, remote_rm, parent_cap)
{
	/* skip loading when called during fork */
	if (!elf_ds.valid())
		return;

	/* attach ELF locally */
	addr_t elf_addr;
	try { elf_addr = local_rm.attach(elf_ds); }
	catch (Region_map::Attach_failed) {
		error("local attach of ELF executable failed"); throw; }

	/* setup ELF object and read program entry pointer */
	Elf_binary elf(elf_addr);
	if (!elf.valid())
		throw Invalid_executable();

	bool const dynamically_linked = elf.dynamically_linked();

	local_rm.detach(elf_addr);

	/*
	 * If the specified executable is a dynamically linked program, we load
	 * the dynamic linker instead.
	 */
	if (dynamically_linked) {

		if (!ldso_ds.valid()) {
			error("attempt to start dynamic executable without dynamic linker");
			throw Missing_dynamic_linker();
		}

		elf_ds = ldso_ds;
	}

	pd.assign_parent(parent_cap);

	Linux_native_pd_client
		lx_pd(static_cap_cast<Linux_native_pd>(pd.native_pd()));

	lx_pd.start(elf_ds);
}


Child::Process::~Process() { }
