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
#include <base/elf.h>
#include <base/env.h>
#include <base/process.h>
#include <base/printf.h>
#include <linux_pd_session/client.h>

/* framework-internal includes */
#include <linux_syscalls.h>


using namespace Genode;

Dataspace_capability Process::_dynamic_linker_cap;


/**
 * Check for dynamic ELF header
 */
static bool _check_dynamic_elf(Dataspace_capability elf_ds_cap)
{
	/* attach ELF locally */
	addr_t elf_addr;

	try { elf_addr = env()->rm_session()->attach(elf_ds_cap); }
	catch (...) { return false; }

	/*
	 * If attach is called within core, it will return zero because
	 * Linux uses Core_rm_session.
	 */
	if (!elf_addr) return false;

	/* read program header and interpreter */
	Elf_binary elf((addr_t)elf_addr);
	env()->rm_session()->detach((void *)elf_addr);

	return elf.is_dynamically_linked();
}


Process::Process(Dataspace_capability   elf_data_ds_cap,
                 Ram_session_capability ram_session_cap,
                 Cpu_session_capability cpu_session_cap,
                 Rm_session_capability  rm_session_cap,
                 Parent_capability      parent_cap,
                 char const            *name,
                 Native_pd_args const  *pd_args)
:
	_pd(name, pd_args),
	_cpu_session_client(cpu_session_cap),
	_rm_session_client(Rm_session_capability())
{
	/* check for dynamic program header */
	if (_check_dynamic_elf(elf_data_ds_cap)) {
		if (!_dynamic_linker_cap.valid()) {
			PERR("Dynamically linked file found, but no dynamic linker binary present");
			return;
		}
		elf_data_ds_cap = _dynamic_linker_cap;
	}

	/*
	 * Register main thread at core
	 *
	 * At this point in time, we do not yet know the TID and PID of the new
	 * thread. Those information will be provided to core by the constructor of
	 * the 'Platform_env' of the new process.
	 */
	enum { WEIGHT = Cpu_session::DEFAULT_WEIGHT };
	_thread0_cap = _cpu_session_client.create_thread(WEIGHT, name);

	Linux_pd_session_client lx_pd(static_cap_cast<Linux_pd_session>(_pd.cap()));

	lx_pd.assign_parent(parent_cap);
	lx_pd.start(elf_data_ds_cap);
}


Process::~Process() { }
