/*
 * \brief  Process creation
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2006-07-18
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/log.h>
#include <base/child.h>
#include <cpu_thread/client.h>

/* base-internal includes */
#include <base/internal/elf.h>
#include <base/internal/parent_cap.h>

using namespace Genode;


Child::Process::Loaded_executable::Loaded_executable(Dataspace_capability elf_ds,
                                                     Dataspace_capability ldso_ds,
                                                     Ram_session &ram,
                                                     Region_map &local_rm,
                                                     Region_map &remote_rm,
                                                     Parent_capability parent_cap)
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

	/*
	 * If the specified executable is a dynamically linked program, we load
	 * the dynamic linker instead.
	 */
	if (elf.dynamically_linked()) {

		local_rm.detach(elf_addr);

		if (!ldso_ds.valid()) {
			error("attempt to start dynamic executable without dynamic linker");
			throw Missing_dynamic_linker();
		}

		try { elf_addr = local_rm.attach(ldso_ds); }
		catch (Region_map::Attach_failed) {
			error("local attach of dynamic linker failed"); throw; }

		elf_ds = ldso_ds;
		elf    = Elf_binary(elf_addr);
	}

	entry = elf.entry();

	/* setup region map for the new pd */
	Elf_segment seg;

	for (unsigned n = 0; (seg = elf.get_segment(n)).valid(); ++n) {
		if (seg.flags().skip)    continue;
		if (seg.mem_size() == 0) continue;

		/* same values for r/o and r/w segments */
		addr_t const addr = (addr_t)seg.start();
		size_t const size = seg.mem_size();

		bool parent_info = false;

		bool const write = seg.flags().w;
		bool const exec = seg.flags().x;

		if (write) {

			/* read-write segment */

			/*
			 * Note that a failure to allocate a RAM dataspace after other
			 * segments were successfully allocated will not revert the
			 * previous allocations. The successful allocations will leak.
			 * In practice, this is not a problem as each component has its
			 * distinct RAM session. When the process creation failed, the
			 * entire RAM session will be destroyed and the memory will be
			 * regained.
			 */

			/* alloc dataspace */
			Dataspace_capability ds_cap;
			try { ds_cap = ram.alloc(size); }
			catch (Ram_session::Alloc_failed) {
				error("allocation of read-write segment failed"); throw; };

			/* attach dataspace */
			void *base;
			try { base = local_rm.attach(ds_cap); }
			catch (Region_map::Attach_failed) {
				error("local attach of segment dataspace failed"); throw; }

			void * const ptr = base;
			addr_t const laddr = elf_addr + seg.file_offset();

			/* copy contents and fill with zeros */
			memcpy(ptr, (void *)laddr, seg.file_size());
			if (size > seg.file_size())
				memset((void *)((addr_t)ptr + seg.file_size()),
				       0, size - seg.file_size());

			/*
			 * We store the parent information at the beginning of the first
			 * data segment
			 */
			if (!parent_info) {
				*(Untyped_capability::Raw *)ptr = parent_cap.raw();
				parent_info = true;
			}

			/* detach dataspace */
			local_rm.detach(base);

			off_t const offset = 0;
			try { remote_rm.attach_at(ds_cap, addr, size, offset); }
			catch (Region_map::Attach_failed) {
				error("remote attach of read-write segment failed"); throw; }

		} else {

			/* read-only segment */

			if (seg.file_size() != seg.mem_size())
				warning("filesz and memsz for read-only segment differ");

			off_t const offset = seg.file_offset();
			try {
				if (exec)
					remote_rm.attach_executable(elf_ds, addr, size, offset);
				else
					remote_rm.attach_at(elf_ds, addr, size, offset);
			}
			catch (Region_map::Attach_failed) {
				error("remote attach of read-only segment failed"); throw; }
		}
	}

	/* detach ELF */
	local_rm.detach((void *)elf_addr);
}


Child::Initial_thread::Initial_thread(Cpu_session          &cpu,
                                      Pd_session_capability pd,
                                      Name           const &name)
:
	_cpu(cpu),
	_cap(cpu.create_thread(pd, name, Affinity::Location(), Cpu_session::Weight()))
{ }


Child::Initial_thread::~Initial_thread()
{
	_cpu.kill_thread(_cap);
}


void Child::Initial_thread::start(addr_t ip)
{
	Cpu_thread_client(_cap).start(ip, 0);
}


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
	/* register parent interface for new protection domain */
	pd.assign_parent(parent_cap);

	/*
	 * Inhibit start of main thread if the new process happens to be forked
	 * from another. In this case, the main thread will get manually
	 * started after constructing the 'Process'.
	 */
	if (!elf_ds.valid())
		return;

	/* start main thread */
	initial_thread.start(loaded_executable.entry);
}


Child::Process::~Process() { }
