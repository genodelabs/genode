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
                                           Local_rm            &local_rm,
                                           Region_map          &remote_rm,
                                           Parent_capability    parent_cap)
{
	Local_rm::Result attached_elf = local_rm.attach(elf_ds, { });

	if (attached_elf == Local_rm::Error::INVALID_DATASPACE)
		error("dynamic linker is an invalid dataspace");
	if (attached_elf == Local_rm::Error::REGION_CONFLICT)
		error("region conflict while attaching dynamic linker");

	addr_t const elf_addr = attached_elf.convert<addr_t>(
		[&] (Local_rm::Attachment &a) { return addr_t(a.ptr); },
		[&] (Local_rm::Error)         { return 0UL; });

	if (!elf_addr)
		return Load_error::INVALID;

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
			Ram_allocator::Result allocated_rw = ram.try_alloc(size);
			if (allocated_rw.failed())
				error("allocation of read-write segment failed");

			if (allocated_rw == Alloc_error::OUT_OF_RAM)  return Load_error::OUT_OF_RAM;
			if (allocated_rw == Alloc_error::OUT_OF_CAPS) return Load_error::OUT_OF_CAPS;
			if (allocated_rw.failed())                    return Load_error::INVALID;

			Ram::Capability ds_cap = allocated_rw.convert<Ram::Capability>(
				[&] (Ram::Allocation &a) { return a.cap; },
				[&] (Alloc_error)        { return Ram::Capability(); });

			/* locally attach dataspace for segment */
			Region_map::Attr attr { };
			attr.writeable = true;
			Local_rm::Result attached_rw = local_rm.attach(ds_cap, attr);

			if (attached_rw == Local_rm::Error::INVALID_DATASPACE)
				error("attempt to attach invalid segment dataspace");
			if (attached_rw == Local_rm::Error::REGION_CONFLICT)
				error("region conflict while locally attaching ELF segment");
			if (attached_rw.failed())
				return Load_error::INVALID;

			attached_rw.with_result([&] (Local_rm::Attachment &dst) {

				/* copy contents and fill with zeros */
				addr_t const src_addr = elf_addr + seg.file_offset();
				memcpy(dst.ptr, (void *)src_addr, seg.file_size());
				if (size > seg.file_size())
					memset((void *)(addr_t(dst.ptr) + seg.file_size()),
					       0, size - seg.file_size());

				/*
				 * We store the parent information at the beginning of the
				 * first data segment
				 */
				if (!parent_info) {
					*(Untyped_capability::Raw *)dst.ptr = parent_cap.raw();
					parent_info = true;
				}
			}, [&] (Local_rm::Error) { });

			auto remote_attach_result = remote_rm.attach(ds_cap, Region_map::Attr {
				.size       = size,   .offset    = { },
				.use_at     = true,   .at        = addr,
				.executable = false,  .writeable = true
			});
			if (remote_attach_result.failed()) {
				error("failed to remotely attach writable ELF segment");
				error("addr=", (void *)addr, " size=", (void *)size);
				return Load_error::INVALID;
			}

			/* keep allocation */
			allocated_rw.with_result([] (Ram::Allocation &a) { a.deallocate = false; },
			                         [] (Alloc_error)        { });
		} else {

			/* read-only segment */

			if (seg.file_size() != seg.mem_size())
				warning("filesz and memsz for read-only segment differ");

			auto remote_attach_result = remote_rm.attach(elf_ds, Region_map::Attr {
				.size       = size,           .offset    = seg.file_offset(),
				.use_at     = true,           .at        = addr,
				.executable = seg.flags().x,  .writeable = false
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
                                          Local_rm              &local_rm,
                                          Region_map            &remote_rm,
                                          Parent_capability      parent_cap)
{
	Pd_ram_allocator ram { pd };
	return _load_static_elf(ldso_ds, ram, local_rm, remote_rm, parent_cap).convert<Start_result>(
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
