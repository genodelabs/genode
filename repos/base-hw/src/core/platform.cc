/*
 * \brief   Platform implementation specific for hw
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2011-12-21
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>

/* core includes */
#include <boot_modules.h>
#include <hw/memory_region.h>
#include <map_local.h>
#include <platform.h>
#include <platform_pd.h>
#include <hw/page_flags.h>
#include <hw/util.h>
#include <pic.h>
#include <kernel/kernel.h>
#include <translation_table.h>
#include <kernel/cpu.h>

/* base-internal includes */
#include <base/internal/crt0.h>
#include <base/internal/stack_area.h>

using namespace Genode;


/**************
 ** Platform **
 **************/

Hw::Boot_info const & Platform::_boot_info() {
	return *reinterpret_cast<Hw::Boot_info*>(Hw::Mm::boot_info().base); }

addr_t Platform::mmio_to_virt(addr_t mmio) {
	return _boot_info().mmio_space.virt_addr(mmio); }

addr_t Platform::core_page_table() {
	return (addr_t)_boot_info().table; }

Hw::Page_table::Allocator & Platform::core_page_table_allocator()
{
	using Allocator  = Hw::Page_table::Allocator;
	using Array      = Allocator::Array<Hw::Page_table::CORE_TRANS_TABLE_COUNT>;
	addr_t virt_addr = Hw::Mm::core_page_tables().base + sizeof(Hw::Page_table);
	return *unmanaged_singleton<Array::Allocator>(_boot_info().table_allocator,
	                                              virt_addr);
}

void Platform::_init_io_mem_alloc()
{
	/* add entire adress space minus the RAM memory regions */
	_io_mem_alloc.add_range(0, ~0x0UL);
	_boot_info().ram_regions.for_each([this] (Hw::Memory_region const &r) {
		_io_mem_alloc.remove_range(r.base, r.size); });
};


Hw::Memory_region_array const & Platform::_core_virt_regions()
{
	return *unmanaged_singleton<Hw::Memory_region_array>(
	Hw::Memory_region(stack_area_virtual_base(), stack_area_virtual_size()));
}


addr_t Platform::core_phys_addr(addr_t virt)
{
	addr_t ret = 0;
	_boot_info().elf_mappings.for_each([&] (Hw::Mapping const & m)
	{
		if (virt >= m.virt() && virt < (m.virt() + m.size()))
			ret = (virt - m.virt()) + m.phys();
	});
	return ret;
}


addr_t Platform::_rom_module_phys(addr_t virt)
{
	Hw::Mapping m = _boot_info().boot_modules;
	if (virt >= m.virt() && virt < (m.virt() + m.size()))
		return (virt - m.virt()) + m.phys();
	return 0;
}


Platform::Platform()
:
	_io_mem_alloc(core_mem_alloc()),
	_io_port_alloc(core_mem_alloc()),
	_irq_alloc(core_mem_alloc())
{
	struct Kernel_resource : Exception { };

	_core_mem_alloc.virt_alloc()->add_range(Hw::Mm::core_heap().base,
	                                        Hw::Mm::core_heap().size);
	_core_virt_regions().for_each([this] (Hw::Memory_region const & r) {
		_core_mem_alloc.virt_alloc()->remove_range(r.base, r.size); });
	_boot_info().elf_mappings.for_each([this] (Hw::Mapping const & m) {
		_core_mem_alloc.virt_alloc()->remove_range(m.virt(), m.size()); });
	_boot_info().ram_regions.for_each([this] (Hw::Memory_region const & region) {
		_core_mem_alloc.phys_alloc()->add_range(region.base, region.size); });

	_init_io_port_alloc();

	/* make all non-kernel interrupts available to the interrupt allocator */
	for (unsigned i = 0; i < Kernel::Pic::NR_OF_IRQ; i++) {
		bool kernel_resource = false;
		Kernel::cpu_pool()->for_each_cpu([&] (Kernel::Cpu const &cpu) {
			if (i == cpu.timer_interrupt_id()) {
				kernel_resource = true;
			}
		});
		if (i == Pic::IPI) {
			kernel_resource = true;
		}
		if (kernel_resource) {
			continue;
		}
		_irq_alloc.add_range(i, 1);
	}

	_init_io_mem_alloc();
	_init_rom_modules();
	_init_additional();

	/* print ressource summary */
	log(":virt_alloc: ",   *_core_mem_alloc.virt_alloc());
	log(":phys_alloc: ",   *_core_mem_alloc.phys_alloc());
	log(":io_mem_alloc: ",  _io_mem_alloc);
	log(":io_port_alloc: ", _io_port_alloc);
	log(":irq_alloc: ",     _irq_alloc);
	log(":rom_fs: ",        _rom_fs);
}


/****************************************
 ** Support for core memory management **
 ****************************************/

bool Genode::map_local(addr_t from_phys, addr_t to_virt, size_t num_pages,
                       Page_flags flags)
{
	Platform_pd * pd = Kernel::core_pd()->platform_pd();
	return pd->insert_translation(to_virt, from_phys,
	                              num_pages * get_page_size(), flags);
}


bool Genode::unmap_local(addr_t virt_addr, size_t num_pages)
{
	Platform_pd * pd = Kernel::core_pd()->platform_pd();
	pd->flush(virt_addr, num_pages * get_page_size());
	return true;
}


bool Mapped_mem_allocator::_map_local(addr_t virt_addr, addr_t phys_addr,
                                      unsigned size) {
	return ::map_local(phys_addr, virt_addr, size / get_page_size()); }


bool Mapped_mem_allocator::_unmap_local(addr_t virt_addr, addr_t phys_addr,
                                        unsigned size) {
	return ::unmap_local(virt_addr, size / get_page_size()); }
