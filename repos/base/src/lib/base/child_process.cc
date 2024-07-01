/*
 * \brief  Process creation
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2006-07-18
 */

/*
 * Copyright (C) 2006-2024 Genode Labs GmbH
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


Child::Load_result Child::_load_static_elf(Dataspace_capability elf_ds,
                                           Ram_allocator       &ram,
                                           Region_map          &local_rm,
                                           Region_map          &remote_rm,
                                           Parent_capability    parent_cap)
{
	addr_t const elf_addr = local_rm.attach(elf_ds, Region_map::Attr{}).convert<addr_t>(
		[&] (Region_map::Range range) { return range.start; },
		[&] (Region_map::Attach_error const e) -> addr_t {
			if (e == Region_map::Attach_error::INVALID_DATASPACE)
				error("dynamic linker is an invalid dataspace");
			if (e == Region_map::Attach_error::REGION_CONFLICT)
				error("region conflict while attaching dynamic linker");
			return 0; });

	if (!elf_addr)
		return Load_error::INVALID;

	/* detach ELF binary from local address space when leaving the scope */
	struct Elf_detach_guard
	{
		Region_map &local_rm;
		addr_t const addr;
		~Elf_detach_guard() { local_rm.detach(addr); }
	} elf_detach_guard { .local_rm = local_rm, .addr = elf_addr };

	Elf_binary elf(elf_addr);

	Entry const entry { elf.entry() };

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
			Ram_allocator::Alloc_result const alloc_result = ram.try_alloc(size);

			if (alloc_result.failed())
				error("allocation of read-write segment failed");

			using Alloc_error = Ram_allocator::Alloc_error;

			if (alloc_result == Alloc_error::OUT_OF_RAM)  return Load_error::OUT_OF_RAM;
			if (alloc_result == Alloc_error::OUT_OF_CAPS) return Load_error::OUT_OF_CAPS;
			if (alloc_result.failed())                    return Load_error::INVALID;

			Dataspace_capability ds_cap = alloc_result.convert<Dataspace_capability>(
				[&] (Ram_dataspace_capability cap)    { return cap; },
				[&] (Alloc_error) { /* handled above */ return Dataspace_capability(); });

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
			if (!ptr)
				return Load_error::INVALID;

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

			auto remote_attach_result = remote_rm.attach(ds_cap, Region_map::Attr {
				.size       = size,
				.offset     = { },
				.use_at     = true,
				.at         = addr,
				.executable = false,
				.writeable  = true
			});
			if (remote_attach_result.failed()) {
				error("failed to remotely attach writable ELF segment");
				error("addr=", (void *)addr, " size=", (void *)size);
				return Load_error::INVALID;
			}
		} else {

			/* read-only segment */

			if (seg.file_size() != seg.mem_size())
				warning("filesz and memsz for read-only segment differ");

			auto remote_attach_result = remote_rm.attach(elf_ds, Region_map::Attr {
				.size       = size,
				.offset     = seg.file_offset(),
				.use_at     = true,
				.at         = addr,
				.executable = seg.flags().x,
				.writeable  = false
			});
			if (remote_attach_result.failed()) {
				error("failed to remotely attach read-only ELF segment");
				error("addr=", (void *)addr, " size=", (void *)size);
				return Load_error::INVALID;
			}
		}
	}
	return entry;
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


Child::Start_result Child::_start_process(Dataspace_capability   ldso_ds,
                                          Pd_session            &pd,
                                          Initial_thread_base   &initial_thread,
                                          Initial_thread::Start &start,
                                          Region_map            &local_rm,
                                          Region_map            &remote_rm,
                                          Parent_capability      parent_cap)
{
	return _load_static_elf(ldso_ds, pd, local_rm, remote_rm, parent_cap).convert<Start_result>(
		[&] (Entry entry) {
			initial_thread.start(entry.ip, start);
			return Start_result::OK;
		},
		[&] (Load_error e)  {
			switch (e) {
			case Load_error::OUT_OF_RAM:  return Start_result::OUT_OF_RAM;
			case Load_error::OUT_OF_CAPS: return Start_result::OUT_OF_CAPS;
			case Load_error::INVALID:     break;
			}
			return Start_result::INVALID;
		}
	);
}
