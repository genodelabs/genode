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


/* core includes */
#include <boot_modules.h>
#include <core_log.h>
#include <irq_root.h>
#include <vm_root.h>

/* base-hw core includes */
#include <map_local.h>
#include <pager.h>
#include <platform.h>
#include <platform_pd.h>
#include <kernel/main.h>

/* base-hw internal includes */
#include <cpu/page_flags.h>
#include <hw/util.h>
#include <hw/memory_region.h>

/* base internal includes */
#include <base/internal/crt0.h>
#include <base/internal/stack_area.h>

/* base includes */
#include <trace/source_registry.h>

using namespace Core;


/**************
 ** Platform **
 **************/

Hw::Boot_info<Board::Boot_info> const & Platform::_boot_info() {
	return *reinterpret_cast<Hw::Boot_info<Board::Boot_info>*>(Hw::Mm::boot_info().base); }


addr_t Platform::mmio_to_virt(addr_t mmio) {
	return _boot_info().mmio_space.virt_addr(mmio); }


addr_t Platform::core_page_table() {
	return (addr_t)_boot_info().table; }


Hw::Page_table::Allocator & Platform::core_page_table_allocator()
{
	using Allocator  = Hw::Page_table::Allocator;
	using Array      = Allocator::Array<Hw::Page_table::CORE_TRANS_TABLE_COUNT>;
	addr_t virt_addr = Hw::Mm::core_page_tables().base + sizeof(Hw::Page_table);

	static Array::Allocator alloc { _boot_info().table_allocator, virt_addr };
	return alloc;
}


addr_t Platform::core_main_thread_phys_utcb()
{
	return core_phys_addr(_boot_info().core_main_thread_utcb);
}


void Platform::_init_io_mem_alloc()
{
	/* add entire adress space minus the RAM memory regions */
	if (_io_mem_alloc.add_range(0, ~0x0UL).failed())
		warning("unable to initialize I/O-memory allocator");

	_boot_info().ram_regions.for_each([this] (unsigned, Hw::Memory_region const &r) {
		if (_io_mem_alloc.remove_range(r.base, r.size).failed())
			warning("unable to exclude I/O range from RAM: ", r); });
};


Hw::Memory_region_array const & Platform::_core_virt_regions()
{
	static Hw::Memory_region_array array {
		Hw::Memory_region(stack_area_virtual_base(), stack_area_virtual_size()) };
	return array;
}


void Platform::Singleton_revoke::revoke_signal_context(Signal_context_capability cap)
{
	if (irq_root) irq_root->revoke_signal_context(cap);
	if (vm_root)  vm_root->revoke_signal_context(cap);
}


addr_t Platform::core_phys_addr(addr_t virt)
{
	addr_t ret = 0;
	_boot_info().elf_mappings.for_each([&] (unsigned, Hw::Mapping const &m)
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


void Platform::_init_platform_info()
{
	unsigned const  pages    = 1;
	size_t   const  rom_size = pages << get_page_size_log2();
	const char     *rom_name = "platform_info";

	Range_allocator::Result phys = ram_alloc().try_alloc(get_page_size());
	Range_allocator::Result virt = region_alloc().try_alloc(rom_size);

	if (phys.failed())
		error("could not setup platform_info ROM - RAM allocation error");
	if (virt.failed())
		error("could not setup platform_info ROM - region allocation error");

	if (phys.failed() || virt.failed())
		return;

	auto start_addr = [&] (Range_allocator::Result &range)
	{
		return range.convert<addr_t>(
			[&] (Range_allocator::Allocation &a) { return addr_t(a.ptr); },
			[]  (Alloc_error)                    { return 0UL; });
	};

	addr_t const phys_addr = start_addr(phys);
	addr_t const virt_addr = start_addr(virt);

	if (!map_local(phys_addr, virt_addr, pages, Hw::PAGE_FLAGS_KERN_DATA)) {
		error("could not setup platform_info ROM - map error");
		return;
	}

	Generator::generate({ reinterpret_cast<char *>(virt_addr), rom_size },
	                    rom_name, [&] (Generator &g)
	{
		g.node("kernel", [&] {
			g.attribute("name", "hw");
			g.attribute("acpi", true);
			g.attribute("msi",  true);
		});
		g.node("board", [&] {
			g.attribute("name", BOARD_NAME);
		});
		_init_additional_platform_info(g);
		g.node("affinity-space", [&] {
			g.attribute("width",  affinity_space().width());
			g.attribute("height", affinity_space().height());
		});
	}).with_error([] (Buffer_error) {
		warning("platform info exceeds maximum buffer size"); });

	if (!unmap_local(virt_addr, pages)) {
		error("could not setup platform_info ROM - unmap error");
		return;
	}

	new (core_mem_alloc()) Rom_module(_rom_fs, rom_name, phys_addr, rom_size);

	/* keep phys allocation but let guard revert virt allocation */
	phys.with_result([&] (auto &allocation) { allocation.deallocate = false; },
	                 []  (Alloc_error) { });
}


Platform::Platform()
:
	_io_mem_alloc(&core_mem_alloc()),
	_io_port_alloc(&core_mem_alloc()),
	_irq_alloc(&core_mem_alloc())
{
	struct Kernel_resource : Exception { };

	if (_core_mem_alloc.virt_alloc().add_range(Hw::Mm::core_heap().base,
	                                           Hw::Mm::core_heap().size).failed())
		warning("unable to initialize core virtual-memory allocator");

	_core_virt_regions().for_each([this] (unsigned, Hw::Memory_region const &r) {
		if (_core_mem_alloc.virt_alloc().remove_range(r.base, r.size).failed())
			warning("unable to exclude core from core's virtual memory"); });

	_boot_info().elf_mappings.for_each([this] (unsigned, Hw::Mapping const &m) {
		if (_core_mem_alloc.virt_alloc().remove_range(m.virt(), m.size()).failed())
			warning("unable to exclude ELF mapping from core's virtual memory"); });

	_boot_info().ram_regions.for_each([this] (unsigned, Hw::Memory_region const &region) {
		if (_core_mem_alloc.phys_alloc().add_range(region.base, region.size).failed())
			warning("unable to register RAM region ", region); });

	_init_io_port_alloc();

	/* make all non-kernel interrupts available to the interrupt allocator */
	for (unsigned i = 0; i < Board::Pic::NR_OF_IRQ; i++) {
		bool kernel_resource = false;
		_boot_info().kernel_irqs.for_each([&] (unsigned /*idx*/,
		                                       unsigned kernel_irq) {
			if (i == kernel_irq) {
				kernel_resource = true;
			}
		});
		if (kernel_resource) {
			continue;
		}
		if (_irq_alloc.add_range(i, 1).failed())
			warning("unable to register IRQ ", i);
	}

	_init_io_mem_alloc();
	_init_rom_modules();
	_init_platform_info();

	/* core log as ROM module */
	{
		unsigned const pages    = 1;
		size_t   const log_size = pages << get_page_size_log2();
		unsigned const align    = get_page_size_log2();

		using Allocation = Range_allocator::Allocation;

		ram_alloc().alloc_aligned(log_size, align).with_result(
			[&] (Allocation &phys) {
				region_alloc().alloc_aligned(log_size, align). with_result(
					[&] (Allocation &virt) {
						addr_t const phys_addr = addr_t(phys.ptr);
						addr_t const virt_addr = addr_t(virt.ptr);

						map_local(phys_addr, virt_addr, pages);
						memset(virt.ptr, 0, log_size);

						new (core_mem_alloc())
							Rom_module(_rom_fs, "core_log", phys_addr, log_size);

						init_core_log(Core_log_range { virt_addr, log_size } );
						phys.deallocate = virt.deallocate = false;
					},
					[&] (Alloc_error) { });
			},
			[&] (Alloc_error) { });
	}

	unsigned const cpus = _boot_info().cpus;
	size_t size = cpus * sizeof(Pager_entrypoint::Thread);
	init_pager_thread_per_cpu_memory(cpus, _core_mem_alloc.alloc(size));

	class Idle_thread_trace_source : public  Trace::Source::Info_accessor,
	                                 private Trace::Control,
	                                 private Trace::Source
	{
		private:

			Affinity::Location const _affinity;

		public:

			/**
			 * Trace::Source::Info_accessor interface
			 */
			Info trace_source_info() const override
			{
				Trace::Execution_time execution_time {
					Kernel::main_read_idle_thread_execution_time(
						_affinity.xpos()), 0 };

				return { Session_label("kernel"), "idle",
				         execution_time, _affinity };
			}

			Idle_thread_trace_source(Trace::Source_registry &registry,
			                         Affinity::Location affinity)
			:
				Trace::Control { },
				Trace::Source  { *this, *this },
				_affinity      { affinity }
			{
				registry.insert(this);
			}
	};

	/* create trace sources for idle threads */
	for (unsigned cpu_idx = 0; cpu_idx < _boot_info().cpus; cpu_idx++) {

		new (core_mem_alloc())
			Idle_thread_trace_source(
				Trace::sources(), Affinity::Location(cpu_idx, 0));
	}
}


/****************************************
 ** Support for core memory management **
 ****************************************/

bool Mapped_mem_allocator::_map_local(addr_t virt, addr_t phys, size_t size) {
	return ::map_local(phys, virt, size / get_page_size()); }


bool Mapped_mem_allocator::_unmap_local(addr_t virt, addr_t, size_t size) {
	return ::unmap_local(virt, size / get_page_size()); }
