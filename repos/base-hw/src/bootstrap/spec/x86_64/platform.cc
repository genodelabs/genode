/*
 * \brief   Platform implementations specific for x86_64
 * \author  Reto Buerki
 * \author  Stefan Kalkowski
 * \date    2015-05-04
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <bios_data_area.h>
#include <platform.h>
#include <multiboot.h>
#include <multiboot2.h>

using namespace Genode;

/* contains Multiboot MAGIC value (either version 1 or 2) */
extern "C" Genode::addr_t __initial_ax;
/* contains physical pointer to multiboot */
extern "C" Genode::addr_t __initial_bx;

Bootstrap::Platform::Board::Board()
: core_mmio(Memory_region { 0, 0x1000 },
            Memory_region { Hw::Cpu_memory_map::MMIO_LAPIC_BASE,
                            Hw::Cpu_memory_map::MMIO_LAPIC_SIZE  },
            Memory_region { Hw::Cpu_memory_map::MMIO_IOAPIC_BASE,
                            Hw::Cpu_memory_map::MMIO_IOAPIC_SIZE },
            Memory_region { __initial_bx & ~0xFFFUL,
                            get_page_size() })
{
	static constexpr size_t initial_map_max = 1024 * 1024 * 1024;

	auto lambda = [&] (addr_t base, addr_t size) {

		/*
		 * Exclude first physical page, so that it will become part of the
		 * MMIO allocator. The framebuffer requests this page as MMIO.
		 */
		if (base == 0 && size >= get_page_size()) {
			base  = get_page_size();
			size -= get_page_size();
		}

		if (base >= initial_map_max) {
			late_ram_regions.add(Memory_region { base, size });
			return;
		}

		if (base + size <= initial_map_max) {
			early_ram_regions.add(Memory_region { base, size });
			return;
		}

		size_t low_size = initial_map_max - base;
		early_ram_regions.add(Memory_region { base, low_size });
		late_ram_regions.add(Memory_region { initial_map_max, size - low_size });
	};

	if (__initial_ax == Multiboot2_info::MAGIC) {
		Multiboot2_info mbi2(__initial_bx);

		mbi2.for_each_tag([&] (Multiboot2_info::Memory const & m) {
			uint32_t const type = m.read<Multiboot2_info::Memory::Type>();

			if (type != Multiboot2_info::Memory::Type::MEMORY)
				return;

			uint64_t const base = m.read<Multiboot2_info::Memory::Addr>();
			uint64_t const size = m.read<Multiboot2_info::Memory::Size>();

			lambda(base, size);
		},
		[&] (Hw::Acpi_rsdp const &rsdp) {
			/* prefer higher acpi revisions */
			if (!acpi_rsdp.valid() || acpi_rsdp.revision < rsdp.revision)
				acpi_rsdp = rsdp;
		},
		[&] (Hw::Framebuffer const &fb) {
			framebuffer = fb;
		});

		return;
	}

	if (__initial_ax != Multiboot_info::MAGIC) {
		error("invalid multiboot magic value: ", Hex(__initial_ax));
		return;
	}

	for (unsigned i = 0; true; i++) {
		using Mmap = Multiboot_info::Mmap;

		Mmap v(Multiboot_info(__initial_bx).phys_ram_mmap_base(i));
		if (!v.base()) break;

		Mmap::Addr::access_t   base = v.read<Mmap::Addr>();
		Mmap::Length::access_t size = v.read<Mmap::Length>();

		lambda(base, size);
	}
}


unsigned Bootstrap::Platform::enable_mmu()
{
	Cpu::Cr3::write(Cpu::Cr3::Pdb::masked((addr_t)core_pd->table_base));
	return 0;
}


addr_t Bios_data_area::_mmio_base_virt() { return 0x1ff000; }


Board::Serial::Serial(addr_t, size_t, unsigned baudrate)
:X86_uart(Bios_data_area::singleton()->serial_port(), 0, baudrate) {}
