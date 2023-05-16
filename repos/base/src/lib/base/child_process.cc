/*
 * \brief  Process creation
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2006-07-18
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>
#include <base/child.h>
#include <cpu_thread/client.h>

/* base-internal includes */
#include <base/internal/elf.h>
#include <base/internal/parent_cap.h>

using namespace Genode;


Child::Process::Loaded_executable::Loaded_executable(Type type,
                                                     Dataspace_capability ldso_ds,
                                                     Ram_allocator &ram,
                                                     Region_map &local_rm,
                                                     Region_map &remote_rm,
                                                     Parent_capability parent_cap)
{
	/* skip loading when called during fork */
	if (type == TYPE_FORKED)
		return;

	/* locally attach ELF binary of the dynamic linker */
	if (!ldso_ds.valid()) {
		error("attempt to start dynamic executable without dynamic linker");
		throw Missing_dynamic_linker();
	}

	addr_t elf_addr = 0;
	try { elf_addr = local_rm.attach(ldso_ds); }
	catch (Region_map::Invalid_dataspace) {
		error("dynamic linker is an invalid dataspace"); throw; }
	catch (Region_map::Region_conflict) {
		error("region conflict while attaching dynamic linker"); throw; }

	Elf_binary elf(elf_addr);

	entry = elf.entry();

	/* setup region map for the new pd */
	Elf_segment seg;

	bool parent_info = false;

	for (unsigned n = 0; (seg = elf.get_segment(n)).valid(); ++n) {
		if (seg.flags().skip)    continue;
		if (seg.mem_size() == 0) continue;

		/* same values for r/o and r/w segments */
		addr_t const addr = (addr_t)seg.start();
		size_t const size = seg.mem_size();

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
			catch (Out_of_ram) {
				error("allocation of read-write segment failed"); throw; };

			/* attach dataspace */
			void *base;
			try { base = local_rm.attach(ds_cap); }
			catch (Region_map::Invalid_dataspace) {
				error("attempt to attach invalid segment dataspace"); throw; }
			catch (Region_map::Region_conflict) {
				error("region conflict while locally attaching ELF segment"); throw; }

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
			catch (Region_map::Region_conflict) {
				error("region conflict while remotely attaching ELF segment");
				error("addr=", (void *)addr, " size=", (void *)size, " offset=", (void *)offset);
				throw; }

		} else {

			/* read-only segment */

			if (seg.file_size() != seg.mem_size())
				warning("filesz and memsz for read-only segment differ");

			off_t const offset = seg.file_offset();
			try {
				if (exec)
					remote_rm.attach_executable(ldso_ds, addr, size, offset);
				else
					remote_rm.attach_at(ldso_ds, addr, size, offset);
			}
			catch (Region_map::Region_conflict) {
				error("region conflict while remotely attaching read-only ELF segment");
				error("addr=", (void *)addr, " size=", (void *)size, " offset=", (void *)offset);
				throw;
			}
			catch (Region_map::Invalid_dataspace) {
				error("attempt to attach invalid read-only segment dataspace");
				throw;
			}
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


void Child::Initial_thread::start(addr_t ip, Start &start)
{
	start.start_initial_thread(_cap, ip);
}


Child::Process::Process(Type                   type,
                        Dataspace_capability   ldso_ds,
                        Pd_session            &pd,
                        Initial_thread_base   &initial_thread,
                        Initial_thread::Start &start,
                        Region_map            &local_rm,
                        Region_map            &remote_rm,
                        Parent_capability      parent_cap)
:
	loaded_executable(type, ldso_ds, pd, local_rm, remote_rm, parent_cap)
{
	/* register parent interface for new protection domain */
	pd.assign_parent(parent_cap);

	/*
	 * Inhibit start of main thread if the new process happens to be forked
	 * from another. In this case, the main thread will get manually
	 * started after constructing the 'Process'.
	 */
	if (type == TYPE_FORKED)
		return;

	/* start main thread */
	initial_thread.start(loaded_executable.entry, start);
}


Child::Process::~Process() { }
