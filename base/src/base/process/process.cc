/*
 * \brief  Process creation
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2006-07-18
 */

/*
 * Copyright (C) 2006-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/process.h>
#include <base/elf.h>
#include <base/env.h>
#include <base/printf.h>
#include <ram_session/client.h>
#include <dataspace/client.h>

using namespace Genode;

Dataspace_capability Process::_dynamic_linker_cap;

/**
 * Check for dynamic ELF header
 *
 * \param elf_ds_cap  dataspace containing the ELF binary
 */

static bool _check_dynamic_elf(Dataspace_capability elf_ds_cap)
{
	/* attach ELF locally */
	addr_t elf_addr;
	try { elf_addr = env()->rm_session()->attach(elf_ds_cap); }
	catch (Rm_session::Attach_failed) { return false; }

	/* read program header */
	Elf_binary elf((addr_t)elf_addr);
	env()->rm_session()->detach((void *)elf_addr);

	return elf.is_dynamically_linked();
}

/**
 * Parse ELF and setup segment dataspace
 *
 * \param parent_cap  parent capability for child (i.e. myself)
 * \param elf_ds_cap  dataspace containing the ELF binary
 * \param ram         RAM session of the new protection domain
 * \param rm          region manager session of the new protection domain
 */
static addr_t _setup_elf(Parent_capability parent_cap,
                         Dataspace_capability elf_ds_cap,
                         Ram_session &ram, Rm_session &rm)
{
	/* attach ELF locally */
	addr_t elf_addr;
	try { elf_addr = env()->rm_session()->attach(elf_ds_cap); }
	catch (Rm_session::Attach_failed) { return 0; }

	/* setup ELF object and read program entry pointer */
	Elf_binary elf((addr_t)elf_addr);
	if (!elf.valid())
		return 0;

	/*
	 * entry point of program - can be set to 0 to indicate errors in ELF
	 * handling
	 */
	addr_t entry = elf.entry();

	/* setup region map for the new pd */
	Elf_segment seg;

	for (unsigned n = 0; (seg = elf.get_segment(n)).valid(); ++n) {
		if (seg.flags().skip) continue;

		/* same values for r/o and r/w segments */
		addr_t addr = (addr_t)seg.start();
		size_t size = seg.mem_size();

		bool parent_info = false;
		off_t  offset;
		Dataspace_capability ds_cap;
		void *out_ptr = 0;

		bool write = seg.flags().w;
		bool exec = seg.flags().x;

		if (write) {

			/* read-write segment */
			offset = 0;

			/* alloc dataspace */
			try { ds_cap = ram.alloc(size); }
			catch (Ram_session::Alloc_failed) {
				PERR("Ram.alloc() failed");
				entry = 0;
				break;
			}

			/* attach dataspace */
			void *base;
			try { base = env()->rm_session()->attach(ds_cap); }
			catch (Rm_session::Attach_failed) {
				PERR("env()->rm_session()->attach() failed");
				entry = 0;
				break;
			}

			void *ptr = base;
			addr_t laddr = elf_addr + seg.file_offset();

			/* copy contents and fill with zeros */
			memcpy(ptr, (void *)laddr, seg.file_size());
			if (size > seg.file_size())
				memset((void *)((addr_t)ptr + seg.file_size()),
				       0, size - seg.file_size());

			/*
			 * we store the parent information at the beginning of the first
			 * data segment
			 */
			if (!parent_info) {
				Native_capability::Raw *raw = (Native_capability::Raw *)ptr;

				raw->dst        = parent_cap.dst();
				raw->local_name = parent_cap.local_name();

				parent_info = true;
			}

			/* detach dataspace */
			env()->rm_session()->detach(base);

			try { out_ptr = rm.attach_at(ds_cap, addr, size, offset); }
			catch (Rm_session::Attach_failed) { }

		} else {

			/* read-only segment */
			offset = seg.file_offset();
			ds_cap = elf_ds_cap;

			/* XXX currently we assume r/o segment sizes never differ */
			if (seg.file_size() != seg.mem_size())
				PWRN("filesz and memsz for read-only segment differ");

			if (exec)
				try { out_ptr = rm.attach_executable(ds_cap, addr, size, offset); }
				catch (Rm_session::Attach_failed) { }
			else
				try { out_ptr = rm.attach_at(ds_cap, addr, size, offset); }
				catch (Rm_session::Attach_failed) { }
		}

		if ((addr_t)out_ptr != addr)
			PWRN("addresses differ after attach (addr=%p out_ptr=%p)",
			     (void *)addr, out_ptr);
	}

	/* detach ELF */
	env()->rm_session()->detach((void *)elf_addr);

	return entry;
}


Process::Process(Dataspace_capability    elf_ds_cap,
                 Ram_session_capability  ram_session_cap,
                 Cpu_session_capability  cpu_session_cap,
                 Rm_session_capability   rm_session_cap,
                 Parent_capability       parent_cap,
                 const char             *name,
                 char *const             argv[])
:
	_cpu_session_client(cpu_session_cap),
	_rm_session_client(rm_session_cap)
{
	if (!_pd.cap().valid())
		return;

	enum Local_exception
	{
		THREAD_FAIL, ELF_FAIL, ASSIGN_PARENT_FAIL, THREAD_ADD_FAIL,
		THREAD_BIND_FAIL, THREAD_PAGER_FAIL, THREAD_START_FAIL,
	};

	/* XXX this only catches local exceptions */

	/* FIXME find sane quota values or make them configurable */
	try {
		int err;

		/* create thread0 */
		try {
			_thread0_cap = _cpu_session_client.create_thread(name);
		} catch (Cpu_session::Thread_creation_failed) {
			PERR("Creation of thread0 failed");
			throw THREAD_FAIL;
		}

		/*
		 * The argument 'elf_ds_cap' may be invalid, which is not an error.
		 * This can happen when the process library is used to set up a process
		 * forked from another. In this case, all process initialization should
		 * be done except for the ELF loading and the startup of the main
		 * thread (as a forked process does not start its execution at the ELF
		 * entrypoint).
		 */
		bool const forked = !elf_ds_cap.valid();

		/* check for dynamic program header */
		if (!forked && _check_dynamic_elf(elf_ds_cap)) {
			if (!_dynamic_linker_cap.valid()) {
				PERR("Dynamically linked file found, but no dynamic linker binary present");
				throw ELF_FAIL;
			}
			elf_ds_cap = _dynamic_linker_cap;
		}

		/* init temporary allocator object */
		Ram_session_client ram(ram_session_cap);

		/* parse ELF binary and setup segment dataspaces */
		addr_t entry = 0;
		if (elf_ds_cap.valid()) {
			entry = _setup_elf(parent_cap, elf_ds_cap, ram, _rm_session_client);
			if (!entry) {
				PERR("Setup ELF failed");
				throw ELF_FAIL;
			}
		}

		/* register parent interface for new protection domain */
		if (_pd.assign_parent(parent_cap)) {
			PERR("Could not assign parent interface to new PD");
			throw ASSIGN_PARENT_FAIL;
		}

		/* bind thread0 */
		err = _pd.bind_thread(_thread0_cap);
		if (err) {
			PERR("Thread binding failed (%d)", err);
			throw THREAD_BIND_FAIL;
		}

		/* register thread0 at region manager session */
		Pager_capability pager;
		try {
			pager = _rm_session_client.add_client(_thread0_cap);
		} catch (...) {
			PERR("Pager setup failed (%d)", err);
			throw THREAD_ADD_FAIL;
		}

		/* set pager in thread0 */
		err = _cpu_session_client.set_pager(_thread0_cap, pager);
		if (err) {
			PERR("Setting pager for thread0 failed");
			throw THREAD_PAGER_FAIL;
		}

		/*
		 * Inhibit start of main thread if the new process happens to be forked
		 * from another. In this case, the main thread will get manually
		 * started after constructing the 'Process'.
		 */
		if (!forked) {

			/* start main thread */
			err = _cpu_session_client.start(_thread0_cap, entry, 0 /* unused */);
			if (err) {
				PERR("Thread0 startup failed");
				throw THREAD_START_FAIL;
			}
		}
	}
	catch (Local_exception cause) {

		switch (cause) {

		case THREAD_START_FAIL:
		case THREAD_PAGER_FAIL:
		case THREAD_ADD_FAIL:
		case THREAD_BIND_FAIL:
		case ASSIGN_PARENT_FAIL:
		case ELF_FAIL:

			_cpu_session_client.kill_thread(_thread0_cap);

		case THREAD_FAIL:

		default:
			PWRN("unknown exception?");
		}
	}
}


Process::~Process()
{
	/*
	 * Try to kill thread0, which was created in the process constructor. If
	 * this fails, do nothing.
	 */
	try { _cpu_session_client.kill_thread(_thread0_cap); }
	catch (Genode::Ipc_error) { }
}
