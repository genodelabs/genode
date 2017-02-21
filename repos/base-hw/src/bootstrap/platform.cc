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

#include <boot_modules.h>
#include <platform.h>

using namespace Bootstrap;


/*****************************
 ** Platform::Ram_allocator **
 *****************************/

void * Platform::Ram_allocator::alloc_aligned(size_t size, unsigned align)
{
	using namespace Genode;
	using namespace Hw;

	void * ret;
	assert(Base::alloc_aligned(round_page(size), &ret,
	                           max(align, get_page_size_log2())).ok());
	return ret;
}


bool Platform::Ram_allocator::alloc(size_t size, void **out_addr)
{
	*out_addr = alloc_aligned(size, 0);
	return true;
}


void Platform::Ram_allocator::add(Memory_region const & region) {
	add_range(region.base, region.size); }


void Platform::Ram_allocator::remove(Memory_region const & region) {
	remove_range(region.base, region.size); }


/******************
 ** Platform::Pd **
 ******************/

Platform::Pd::Pd(Platform::Ram_allocator & alloc)
: table_base(alloc.alloc_aligned(sizeof(Table),       Table::ALIGNM_LOG2)),
  array_base(alloc.alloc_aligned(sizeof(Table_array), Table::ALIGNM_LOG2)),
  table(*Genode::construct_at<Table>(table_base)),
  array(*Genode::construct_at<Table_array>(array_base))
{
	using namespace Genode;
	map_insert(Mapping((addr_t)table_base, (addr_t)table_base,
	                   sizeof(Table),       Hw::PAGE_FLAGS_KERN_DATA));
	map_insert(Mapping((addr_t)array_base, (addr_t)array_base,
	                   sizeof(Table_array), Hw::PAGE_FLAGS_KERN_DATA));
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

addr_t Platform::_load_elf()
{
	using namespace Genode;
	using namespace Hw;

	addr_t start = ~0UL;
	addr_t end   = 0;
	auto lambda = [&] (Genode::Elf_segment & segment) {
		void * phys       = (void*)(core_elf_addr + segment.file_offset());
		start             = min(start, (addr_t) phys);
		size_t const size = round_page(segment.mem_size());

		if (segment.flags().w) {
			unsigned align_log2;
			for (align_log2 = 0; align_log2 < 8*sizeof(addr_t); align_log2++)
				if ((addr_t)(1 << align_log2) & (addr_t)phys) break;

			void * const dst = ram_alloc.alloc_aligned(segment.mem_size(),
			                                           align_log2);
			memcpy(dst, phys, segment.file_size());

			if (size > segment.file_size())
				memset((void *)((addr_t)dst + segment.file_size()),
				       0, size - segment.file_size());

			phys = dst;
		}

		//FIXME: set read-only, privileged and global accordingly
		Page_flags flags{RW, segment.flags().x ? EXEC : NO_EXEC,
		                 USER, NO_GLOBAL, RAM, CACHED};
		Mapping m((addr_t)phys, (addr_t)segment.start(), size, flags);
		core_pd->map_insert(m);
		end = max(end, (addr_t)segment.start() + size);
	};
	core_elf.for_each_segment(lambda);
	return end;
}


void Platform::start_core()
{
	typedef void (* Entry)();
	Entry __attribute__((noreturn)) const entry
		= reinterpret_cast<Entry>(core_elf.entry());
	entry();
}


static constexpr Genode::Boot_modules_header & header() {
	return *((Genode::Boot_modules_header*) &_boot_modules_headers_begin); }


Platform::Platform()
: bootstrap_region((addr_t)&_prog_img_beg,
                   ((addr_t)&_prog_img_end - (addr_t)&_prog_img_beg)),
  core_pd(ram_alloc),
  core_elf_addr(header().base),
  core_elf(core_elf_addr)
{
	using namespace Genode;

	/* prepare the ram allocator */
	board.early_ram_regions.for_each([this] (Memory_region const & region) {
		ram_alloc.add(region); });
	ram_alloc.remove(bootstrap_region);

	/* now we can use the ram allocator for core's pd */
	core_pd.construct(ram_alloc);

	/* temporarily map all bootstrap memory 1:1 for transition to core */
	// FIXME do not insert as mapping for core
	core_pd->map_insert(Mapping(bootstrap_region.base, bootstrap_region.base,
	                            bootstrap_region.size, Hw::PAGE_FLAGS_KERN_TEXT));

	/* map memory-mapped I/O for core */
	board.core_mmio.for_each_mapping([&] (Mapping const & m) {
		core_pd->map_insert(m); });

	/* load ELF */
	addr_t const elf_end = _load_elf();

	/* setup boot info page */
	void * bi_base = ram_alloc.alloc(sizeof(Boot_info));
	core_pd->map_insert(Mapping((addr_t)bi_base, elf_end, sizeof(Boot_info),
								Hw::PAGE_FLAGS_KERN_TEXT));
	Boot_info & bootinfo =
		*construct_at<Boot_info>(bi_base, (addr_t)&core_pd->table,
		                        (addr_t)&core_pd->array,
		                        core_pd->mappings, board.core_mmio);

	/* add all left RAM to bootinfo */
	ram_alloc.for_each_free_region([&] (Memory_region const & r) {
		bootinfo.ram_regions.add(r); });
	board.late_ram_regions.for_each([&] (Memory_region const & r) {
		bootinfo.ram_regions.add(r); });
}
