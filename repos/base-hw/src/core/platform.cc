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
#include <memory_region.h>
#include <core_parent.h>
#include <map_local.h>
#include <platform.h>
#include <platform_pd.h>
#include <page_flags.h>
#include <util.h>
#include <pic.h>
#include <kernel/kernel.h>
#include <translation_table.h>
#include <trustzone.h>

/* base-internal includes */
#include <base/internal/crt0.h>
#include <base/internal/stack_area.h>

using namespace Genode;

void __attribute__((weak)) Kernel::init_trustzone(Pic & pic) { }


/**************
 ** Platform **
 **************/

Bootinfo const & Platform::_bootinfo() {
	return *reinterpret_cast<Bootinfo*>(round_page((addr_t)&_prog_img_end)); }

addr_t Platform::mmio_to_virt(addr_t mmio) {
	return _bootinfo().core_mmio.virt_addr(mmio); }

Translation_table * Platform::core_translation_table() {
	return _bootinfo().table; }

Translation_table_allocator * Platform::core_translation_table_allocator() {
	return _bootinfo().table_allocator->alloc(); }

void Platform::_init_io_mem_alloc()
{
	/* add entire adress space minus the RAM memory regions */
	_io_mem_alloc.add_range(0, ~0x0UL);
	_bootinfo().ram_regions.for_each([this] (Memory_region const &r) {
		_io_mem_alloc.remove_range(r.base, r.size); });
};


Memory_region_array const & Platform::_core_virt_regions()
{
	return *unmanaged_singleton<Memory_region_array>(
	Memory_region(stack_area_virtual_base(), stack_area_virtual_size()));
}


addr_t Platform::core_phys_addr(addr_t virt)
{
	addr_t ret = 0;
	_bootinfo().elf_mappings.for_each([&] (Mapping const & m)
	{
		if (virt >= m.virt() && virt < (m.virt() + m.size()))
			ret = (virt - m.virt()) + m.phys();
	});
	return ret;
}


Platform::Platform()
:
	_io_mem_alloc(core_mem_alloc()),
	_io_port_alloc(core_mem_alloc()),
	_irq_alloc(core_mem_alloc())
{
	_core_mem_alloc.virt_alloc()->add_range(VIRT_ADDR_SPACE_START,
	                                        VIRT_ADDR_SPACE_SIZE);
	_core_virt_regions().for_each([this] (Memory_region const & r) {
		_core_mem_alloc.virt_alloc()->remove_range(r.base, r.size); });
	_bootinfo().elf_mappings.for_each([this] (Mapping const & m) {
		_core_mem_alloc.virt_alloc()->remove_range(m.virt(), m.size()); });
	_bootinfo().ram_regions.for_each([this] (Memory_region const & region) {
		_core_mem_alloc.phys_alloc()->add_range(region.base, region.size); });

	_init_io_port_alloc();

	/* make all non-kernel interrupts available to the interrupt allocator */
	for (unsigned i = 0; i < Kernel::Pic::NR_OF_IRQ; i++) {
		if (i == Timer::interrupt_id(i) || i == Pic::IPI)
			continue;
		_irq_alloc.add_range(i, 1);
	}

	_init_io_mem_alloc();

	/* add boot modules to ROM FS */
	Boot_modules_header * header = &_boot_modules_headers_begin;
	for (; header < &_boot_modules_headers_end; header++) {
		Rom_module * rom_module = new (core_mem_alloc())
			Rom_module(Platform::core_phys_addr(header->base), header->size,
			           (const char*)header->name);
		_rom_fs.insert(rom_module);
	}

	_init_additional();

	/* print ressource summary */
	log(":virt_alloc: ",   *_core_mem_alloc.virt_alloc());
	log(":phys_alloc: ",   *_core_mem_alloc.phys_alloc());
	log(":io_mem_alloc: ",  _io_mem_alloc);
	log(":io_port_alloc: ", _io_port_alloc);
	log(":irq_alloc: ",     _irq_alloc);
	log(":rom_fs: ",        _rom_fs);
}


/*****************
 ** Core_parent **
 *****************/

void Core_parent::exit(int exit_value)
{
	warning(__PRETTY_FUNCTION__, "not implemented");
	while (1);
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
