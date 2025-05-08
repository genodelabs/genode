/*
 * \brief  Platform implementation
 * \author Stefan Kalkowski
 * \date   2016-10-19
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/internal/crt0.h>
#include <hw/assert.h>
#include <hw/memory_consts.h>

#include <boot_modules.h>
#include <platform.h>

using namespace Bootstrap;


extern unsigned _bss_end;


/*****************************
 ** Platform::Ram_allocator **
 *****************************/

void * Platform::Ram_allocator::alloc(size_t size, Align align)
{
	using namespace Genode;

	align.log2 = max(align.log2, (uint8_t)get_page_size_log2());

	return Base::alloc_aligned(Hw::round_page(size), align.log2).convert<void *>(

		[&] (Range_allocator::Allocation &a) { a.deallocate = false; return a.ptr; },
		[&] (Alloc_error e) -> void *
		{
			error("bootstrap RAM allocation failed, error=", e);
			assert(false);
		});
}


void Platform::Ram_allocator::add(Memory_region const & region)
{
	if (add_range(region.base, region.size).failed())
		Genode::warning("bootstrap failed to register RAM: ", region);
}


void Platform::Ram_allocator::remove(Memory_region const & region)
{
	if (remove_range(region.base, region.size).failed())
		Genode::warning("bootstrap unable to exclude RAM: ", region);
}


/******************
 ** Platform::Pd **
 ******************/

Platform::Pd::Pd(Platform::Ram_allocator & alloc)
:
	table_base(alloc.alloc(sizeof(Table),       { Table::ALIGNM_LOG2 })),
	array_base(alloc.alloc(sizeof(Table_array), { Table::ALIGNM_LOG2 })),
	table(*Genode::construct_at<Table>(table_base)),
	array(*Genode::construct_at<Table_array>(array_base))
{
	using namespace Genode;
	addr_t const table_virt_base = Hw::Mm::core_page_tables().base;
	map_insert(Mapping((addr_t)table_base, table_virt_base,
	                   sizeof(Table),       Genode::PAGE_FLAGS_KERN_DATA));
	map_insert(Mapping((addr_t)array_base, table_virt_base + sizeof(Table),
	                   sizeof(Table_array), Genode::PAGE_FLAGS_KERN_DATA));
}


void Platform::Pd::map(Mapping m)
{
	try {
		table.insert_translation(m.virt(), m.phys(), m.size(), m.flags(),
		                         array.alloc());
	} catch (Hw::Out_of_tables &) {
		Genode::error("translation table needs to much RAM");
	} catch (...) {
		Genode::error("invalid mapping ", m);
	}
}


void Platform::Pd::map_insert(Mapping m)
{
	mappings.add(m);
	map(m);
}


/**************
 ** Platform **
 **************/

Mapping Platform::_load_elf()
{
	using namespace Genode;
	using namespace Hw;

	Mapping ret;
	auto lambda = [&] (Genode::Elf_segment & segment) {
		void * phys       = (void*)(core_elf_addr + segment.file_offset());
		size_t const size = round_page(segment.mem_size());

		if (segment.flags().w) {
			uint8_t align_log2;
			for (align_log2 = 0; align_log2 < 8*sizeof(addr_t); align_log2++)
				if ((addr_t)(1 << align_log2) & (addr_t)phys) break;

			void * const dst = ram_alloc.alloc(segment.mem_size(), { align_log2 });
			memcpy(dst, phys, segment.file_size());

			if (size > segment.file_size())
				memset((void *)((addr_t)dst + segment.file_size()),
				       0, size - segment.file_size());

			phys = dst;
		}

		Page_flags flags{segment.flags().w ? RW : RO,
		                 segment.flags().x ? EXEC : NO_EXEC,
		                 KERN, GLOBAL, RAM, CACHED};
		Mapping m((addr_t)phys, (addr_t)segment.start(), size, flags);

		/*
		 * Do not map the read-only, non-executable segment containing
		 * the boot modules, although it is a loadable segment, which we
		 * define so that the modules are loaded as ELF image
		 * via the bootloader
		 */
		if (segment.flags().x || segment.flags().w)
			core_pd->map_insert(m);
		else
			ret = m;

		/* map start of the text segment as exception vector */
		if (segment.flags().x && !segment.flags().w) {
			Memory_region e = Hw::Mm::supervisor_exception_vector();
			core_pd->map_insert(Mapping((addr_t)phys, e.base, e.size, flags));
		}
	};
	core_elf.for_each_segment(lambda);
	return ret;
}


void Platform::_prepare_cpu_memory_area(size_t cpu_id)
{
	using namespace Genode;
	using namespace Hw;
	using namespace Hw::Mm;

	size_t slots = cpu_local_memory().size / CPU_LOCAL_MEMORY_SLOT_SIZE;

	if (cpu_id >= slots) {
		error("CPU memory area too small for cpu id ", cpu_id);
		error("CPU memory area can hold ", slots, " at max");
		return;
	}

	Page_flags flags{RW, NO_EXEC, KERN, GLOBAL, RAM, CACHED};

	addr_t base = cpu_local_memory().base + CPU_LOCAL_MEMORY_SLOT_SIZE*cpu_id;
	void * const stack_ram = ram_alloc.alloc(KERNEL_STACK_SIZE, { });
	void * const cpu_ram   = ram_alloc.alloc(CPU_LOCAL_MEMORY_SLOT_OBJECT_SIZE, { });

	core_pd->map_insert(Mapping((addr_t)stack_ram,
	                            base+CPU_LOCAL_MEMORY_SLOT_STACK_OFFSET,
	                            KERNEL_STACK_SIZE, flags));
	core_pd->map_insert(Mapping((addr_t)cpu_ram,
	                            base+CPU_LOCAL_MEMORY_SLOT_OBJECT_OFFSET,
	                            CPU_LOCAL_MEMORY_SLOT_OBJECT_SIZE, flags));
}


void Platform::start_core(unsigned cpu_id)
{
	typedef void (* Entry)(unsigned) __attribute__((noreturn));
	Entry const entry = reinterpret_cast<Entry>(core_elf.entry());
	entry(cpu_id);
}


static constexpr Core::Boot_modules_header &header()
{
	return *((Core::Boot_modules_header*) &_boot_modules_headers_begin);
}


Platform::Platform()
:
	bootstrap_region((addr_t)&_prog_img_beg,
	                 ((addr_t)&_prog_img_end - (addr_t)&_prog_img_beg)),
	core_elf_addr(header().base),
	core_elf(core_elf_addr)
{
	using namespace Genode;

	/* prepare the ram allocator */
	board.early_ram_regions.for_each([this] (unsigned, Memory_region const & region) {
		ram_alloc.add(region); });
	ram_alloc.remove(bootstrap_region);

	/* now we can use the ram allocator for core's pd */
	core_pd.construct(ram_alloc);

	/* temporarily map all bootstrap memory 1:1 for transition to core */
	// FIXME do not insert as mapping for core
	core_pd->map_insert(Mapping(bootstrap_region.base, bootstrap_region.base,
	                            (addr_t)&_bss_end - (addr_t)&_prog_img_beg, Genode::PAGE_FLAGS_KERN_TEXT));

	board.cpus = _prepare_cpu_memory_area();

	/* map memory-mapped I/O for core */
	board.core_mmio.for_each_mapping([&] (Mapping const & m) {
		core_pd->map_insert(m); });

	/* load ELF */
	Mapping boot_modules = _load_elf();

	/* setup boot info page */
	void * bi_base = ram_alloc.alloc(sizeof(Boot_info), { });
	core_pd->map_insert(Mapping((addr_t)bi_base, Hw::Mm::boot_info().base,
	                            sizeof(Boot_info), Genode::PAGE_FLAGS_KERN_TEXT));
	Boot_info & bootinfo =
		*construct_at<Boot_info>(bi_base, (addr_t)&core_pd->table,
		                         (addr_t)&core_pd->array,
		                         core_pd->mappings, boot_modules,
		                         board.core_mmio, board.cpus, board.info);

	/* add all left RAM to bootinfo */
	ram_alloc.for_each_free_region([&] (Memory_region const & r) {
		bootinfo.ram_regions.add(r); });
	board.late_ram_regions.for_each([&] (unsigned, Memory_region const & r) {
		/* is zero initialized on some platforms */
		if (r.size) bootinfo.ram_regions.add(r); });
}
