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

	addr_t const elf_addr = local_rm.attach(ldso_ds, Region_map::Attr{}).convert<addr_t>(
		[&] (Region_map::Range range) { return range.start; },
		[&] (Region_map::Attach_error const e) -> addr_t {
			if (e == Region_map::Attach_error::INVALID_DATASPACE)
				error("dynamic linker is an invalid dataspace");
			if (e == Region_map::Attach_error::REGION_CONFLICT)
				error("region conflict while attaching dynamic linker");
			return 0; });

	if (!elf_addr)
		return;

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
			Region_map::Attr attr { };
			attr.writeable = true;
			void * const ptr = local_rm.attach(ds_cap, attr).convert<void *>(
				[&] (Region_map::Range range) { return (void *)range.start; },
				[&] (Region_map::Attach_error const e) {
					if (e == Region_map::Attach_error::INVALID_DATASPACE)
						error("attempt to attach invalid segment dataspace");
					if (e == Region_map::Attach_error::REGION_CONFLICT)
						error("region conflict while locally attaching ELF segment");
					return nullptr; });

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
			local_rm.detach(addr_t(ptr));

			remote_rm.attach(ds_cap, Region_map::Attr {
				.size       = size,
				.offset     = { },
				.use_at     = true,
				.at         = addr,
				.executable = false,
				.writeable  = true
			}).with_result(
				[&] (Region_map::Range) { },
				[&] (Region_map::Attach_error) {
					error("region conflict while remotely attaching ELF segment");
					error("addr=", (void *)addr, " size=", (void *)size); }
			);
		} else {

			/* read-only segment */

			if (seg.file_size() != seg.mem_size())
				warning("filesz and memsz for read-only segment differ");

			remote_rm.attach(ldso_ds, Region_map::Attr {
				.size       = size,
				.offset     = seg.file_offset(),
				.use_at     = true,
				.at         = addr,
				.executable = seg.flags().x,
				.writeable  = false
			}).with_result(
				[&] (Region_map::Range) { },
				[&] (Region_map::Attach_error const e) {
					if (e == Region_map::Attach_error::REGION_CONFLICT)
						error("region conflict while remotely attaching read-only ELF segment");
					if (e == Region_map::Attach_error::INVALID_DATASPACE)
						error("attempt to attach invalid read-only segment dataspace");
					error("addr=", (void *)addr, " size=", (void *)size);
				}
			);
		}
	}

	/* detach ELF */
	local_rm.detach(elf_addr);
}


static Thread_capability create_thread(auto &pd, auto &cpu, auto const &name)
{
	return cpu.create_thread(pd, name, { }, { }).template convert<Thread_capability>(
		[&] (Thread_capability cap) { return cap; },
		[&] (Cpu_session::Create_thread_error) {
			error("failed to create initial thread for new PD");
			return Thread_capability(); });
}


Child::Initial_thread::Initial_thread(Cpu_session          &cpu,
                                      Pd_session_capability pd,
                                      Name           const &name)
:
	_cpu(cpu), _cap(create_thread(pd, cpu, name))
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
