/*
 * \brief   Platform implementations specific for x86_64
 * \author  Reto Buerki
 * \author  Stefan Kalkowski
 * \author  Alexander Boettcher
 * \date    2015-05-04
 */

/*
 * Copyright (C) 2015-2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <bios_data_area.h>
#include <platform.h>
#include <multiboot.h>
#include <multiboot2.h>

#include <hw/spec/x86_64/acpi.h>

using namespace Genode;


/* contains Multiboot MAGIC value (either version 1 or 2) */
extern "C" Genode::addr_t __initial_ax;

/* contains physical pointer to multiboot */
extern "C" Genode::addr_t __initial_bx;

/* pointer to stack base */
extern "C" Genode::addr_t bootstrap_stack;

/* number of booted CPUs */
extern "C" Genode::addr_t volatile __cpus_booted;

/* stack size per CPU */
extern "C" Genode::addr_t const bootstrap_stack_size;


/* hardcoded physical page or AP CPUs boot code */
enum { AP_BOOT_CODE_PAGE = 0x1000 };

extern "C" void * _start;
extern "C" void * _ap;


static Hw::Acpi_rsdp search_rsdp(addr_t area, addr_t area_size)
{
	if (area && area_size && area < area + area_size) {
		for (addr_t addr = 0; addr + sizeof(Hw::Acpi_rsdp) <= area_size;
		     addr += sizeof(Hw::Acpi_rsdp::signature))
		{
			Hw::Acpi_rsdp * rsdp = reinterpret_cast<Hw::Acpi_rsdp *>(area + addr);
			if (rsdp->valid())
				return *rsdp;
		}
	}

	Hw::Acpi_rsdp invalid;
	return invalid;
}


static Genode::uint8_t apic_id()
{
	X86_64_CPUID_REGISTER(Cpuid_1_ebx, 1, ebx,
		struct Apic_id : Bitfield<24, 8> { };
	);

	return uint8_t(Cpuid_1_ebx::Apic_id::get(Cpuid_1_ebx::read()));
}


unsigned apic_to_cpu_id(Genode::uint8_t apic_id, Genode::uint8_t dense_id)
{
	static Genode::uint8_t cpu_id[256] { };
	static bool            valid [256] { };

	/* if apic_id is valid, return stored cpu_id and ignore new dense_id */
	if (!valid[apic_id]) {
		cpu_id[apic_id] = dense_id;
		valid [apic_id] = true;
	}

	return cpu_id[apic_id];
}


Bootstrap::Platform::Board::Board()
:
	core_mmio(Memory_region { 0, 0x1000 },
	          Memory_region { Hw::Cpu_memory_map::lapic_phys_base(), 0x1000 },
	          Memory_region { Hw::Cpu_memory_map::MMIO_IOAPIC_BASE,
	                          Hw::Cpu_memory_map::MMIO_IOAPIC_SIZE },
	          Memory_region { __initial_bx & ~0xFFFUL,
	                          get_page_size() })
{
	Hw::Acpi_rsdp & acpi_rsdp = info.acpi_rsdp;
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

		/* exclude AP boot code page from normal RAM allocator */
		if (base <= AP_BOOT_CODE_PAGE && AP_BOOT_CODE_PAGE < base + size) {
			if (AP_BOOT_CODE_PAGE - base)
				early_ram_regions.add(Memory_region { base,
				                                      AP_BOOT_CODE_PAGE - base });

			size -= AP_BOOT_CODE_PAGE - base;
			size -= (get_page_size() > size) ? size : get_page_size();
			base  = AP_BOOT_CODE_PAGE + get_page_size();
		}

		/* skip partial 4k pages (seen with Qemu with ahci model enabled) */
		if (!aligned(base, 12)) {
			auto new_base = align_addr(base, 12);
			size -= (new_base - base > size) ? size : new_base - base;
			base  = new_base;
		}

		/* remove partial 4k pages */
		if (!aligned(size, 12))
			size -= size & 0xffful;

		if (!size)
			return;

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
			info.framebuffer = fb;
		},
		[&] (uint64_t const efi_sys_tab) {
			info.efi_system_table = efi_sys_tab;
		});
	} else if (__initial_ax == Multiboot_info::MAGIC) {
		for (unsigned i = 0; true; i++) {
			using Mmap = Multiboot_info::Mmap;

			Mmap v(Multiboot_info(__initial_bx).phys_ram_mmap_base(i));
			if (!v.base()) break;

			Mmap::Addr::access_t   base = v.read<Mmap::Addr>();
			Mmap::Length::access_t size = v.read<Mmap::Length>();

			lambda(base, size);
		}

		/* search ACPI RSDP pointer at known places */

		/* BIOS range to scan for */
		enum { BIOS_BASE = 0xe0000, BIOS_SIZE = 0x20000 };
		acpi_rsdp = search_rsdp(BIOS_BASE, BIOS_SIZE);

		if (!acpi_rsdp.valid()) {
			/* page 0 is remapped to 2M - 4k - see crt_translation table */
			addr_t const bios_addr = 2 * 1024 * 1024 - 4096;

			/* search EBDA (BIOS addr + 0x40e) */
			addr_t ebda_phys = (*reinterpret_cast<uint16_t *>(bios_addr + 0x40e)) << 4;
			if (ebda_phys < 0x1000)
				ebda_phys = bios_addr;

			acpi_rsdp = search_rsdp(ebda_phys, 0x1000 /* EBDA size */);
		}
	} else {
		error("invalid multiboot magic value: ", Hex(__initial_ax));
	}

	/* remember max supported CPUs and use ACPI to get the actual number */
	unsigned const max_cpus = cpus;
	cpus = 0;

	/* scan ACPI tables to find out number of CPUs in this machine */
	if (acpi_rsdp.valid()) {
		uint64_t const table_addr = acpi_rsdp.xsdt ? acpi_rsdp.xsdt : acpi_rsdp.rsdt;

		if (table_addr) {
			auto rsdt_xsdt_lambda = [&](auto paddr_table) {
				addr_t const table_virt_addr = paddr_table;
				Hw::Acpi_generic * table = reinterpret_cast<Hw::Acpi_generic *>(table_virt_addr);

				if (!memcmp(table->signature, "FACP", 4)) {
					info.acpi_fadt = addr_t(table);

					Hw::Acpi_fadt fadt(table);
					Hw::Acpi_facs facs(fadt.facs());
					facs.wakeup_vector(AP_BOOT_CODE_PAGE);

					auto mem_aligned = paddr_table & _align_mask(12);
					auto mem_size    = align_addr(paddr_table + table->size, 12) - mem_aligned;
					core_mmio.add({ mem_aligned, mem_size });
				}

				if (memcmp(table->signature, "APIC", 4))
					return;

				Hw::for_each_apic_struct(*table,[&](Hw::Apic_madt const *e){
					if (e->type == Hw::Apic_madt::LAPIC) {
						Hw::Apic_madt::Lapic lapic(e);

						/* check if APIC is enabled in hardware */
						if (lapic.valid())
							cpus ++;
					}
				});
			};

			auto table = reinterpret_cast<Hw::Acpi_generic *>(table_addr);
			if (!memcmp(table->signature, "RSDT", 4)) {
				Hw::for_each_rsdt_entry(*table, rsdt_xsdt_lambda);
			} else if (!memcmp(table->signature, "XSDT", 4)) {
				Hw::for_each_xsdt_entry(*table, rsdt_xsdt_lambda);
			}
		}
	}

	if (!cpus || cpus > max_cpus) {
		Genode::warning("CPU count is unsupported ", cpus, "/", max_cpus,
		                acpi_rsdp.valid() ? " - invalid or missing RSDT/XSDT"
		                                  : " - invalid RSDP");
		cpus = !cpus ? 1 : max_cpus;
	}

	/* copy 16 bit boot code for AP CPUs and for ACPI resume */
	addr_t ap_code_size = (addr_t)&_start - (addr_t)&_ap;
	memcpy((void *)AP_BOOT_CODE_PAGE, &_ap, ap_code_size);
}


struct Lapic : Mmio<Hw::Cpu_memory_map::LAPIC_SIZE>
{
	struct Svr : Register<0x0f0, 32>
	{
		struct APIC_enable : Bitfield<8, 1> { };
	};

	struct Icr_low : Register<0x300, 32>
	{
		struct Vector          : Bitfield< 0, 8> { };
		struct Delivery_mode   : Bitfield< 8, 3>
		{
			enum Mode { INIT = 5, SIPI = 6 };
		};
		struct Delivery_status : Bitfield<12, 1> { };
		struct Level_assert    : Bitfield<14, 1> { };
		struct Dest_shorthand  : Bitfield<18, 2>
		{
			enum { ALL_OTHERS = 3 };
		};
	};

	struct Icr_high : Register<0x310, 32>
	{
		struct Destination : Bitfield<24, 8> { };
	};

	Lapic(addr_t const addr) : Mmio({(char *)addr, Mmio::SIZE}) { }
};


static inline void ipi_to_all(Lapic &lapic, unsigned const boot_frame,
                              Lapic::Icr_low::Delivery_mode::Mode const mode)
{
	/* wait until ready */
	while (lapic.read<Lapic::Icr_low::Delivery_status>())
		asm volatile ("pause":::"memory");

	unsigned const apic_cpu_id = 0; /* unused for IPI to all */

	Lapic::Icr_low::access_t icr_low = 0;

	Lapic::Icr_low::Vector::set(icr_low, boot_frame);
	Lapic::Icr_low::Delivery_mode::set(icr_low, mode);
	Lapic::Icr_low::Level_assert::set(icr_low);
	Lapic::Icr_low::Level_assert::set(icr_low);
	Lapic::Icr_low::Dest_shorthand::set(icr_low, Lapic::Icr_low::Dest_shorthand::ALL_OTHERS);

	/* program */
	lapic.write<Lapic::Icr_high::Destination>(apic_cpu_id);
	lapic.write<Lapic::Icr_low>(icr_low);
}


unsigned Bootstrap::Platform::enable_mmu()
{
	using ::Board::Cpu;

	/* enable PAT if available */
	Cpu::Cpuid_1_edx::access_t cpuid1 = Cpu::Cpuid_1_edx::read();
	if (Cpu::Cpuid_1_edx::Pat::get(cpuid1)) {
		Cpu::IA32_pat::access_t pat = Cpu::IA32_pat::read();
		if (Cpu::IA32_pat::Pa1::get(pat) != Cpu::IA32_pat::Pa1::WRITE_COMBINING) {
			Cpu::IA32_pat::Pa1::set(pat, Cpu::IA32_pat::Pa1::WRITE_COMBINING);
			Cpu::IA32_pat::write(pat);
		}
	}

	Cpu::Cr3::write(Cpu::Cr3::Pdb::masked((addr_t)core_pd->table_base));

	addr_t const stack_base = reinterpret_cast<addr_t>(&bootstrap_stack);
	addr_t const this_stack = reinterpret_cast<addr_t>(&stack_base);
	addr_t const stack_id   = (this_stack - stack_base) / bootstrap_stack_size;

	/* determine dense packed cpu_id based on apic_id */
	auto const cpu_id = apic_to_cpu_id(apic_id(), uint8_t(stack_id));

	/* we like to use local APIC */
	Cpu::IA32_apic_base::access_t lapic_msr = Cpu::IA32_apic_base::read();
	Cpu::IA32_apic_base::Lapic::set(lapic_msr);
	Cpu::IA32_apic_base::write(lapic_msr);

	Lapic lapic(board.core_mmio.virt_addr(Hw::Cpu_memory_map::lapic_phys_base()));

	/* enable local APIC if required */
	if (!lapic.read<Lapic::Svr::APIC_enable>())
		lapic.write<Lapic::Svr::APIC_enable>(true);

	/* reset assembly counter (crt0.s) by last booted CPU, required for resume */
	if (__cpus_booted >= board.cpus)
		__cpus_booted = 0;

	/* skip wakeup IPI for non SMP setups */
	if (board.cpus <= 1)
		return (unsigned)cpu_id;

	if (!Cpu::IA32_apic_base::Bsp::get(lapic_msr))
		/* AP - done */
		return (unsigned)cpu_id;

	/* BSP - we're primary CPU - wake now all other CPUs */

	/* see Intel Multiprocessor documentation - we need to do INIT-SIPI-SIPI */
	ipi_to_all(lapic, 0 /* unused */, Lapic::Icr_low::Delivery_mode::INIT);
	/* wait 10  ms - debates ongoing whether this is still required */
	ipi_to_all(lapic, AP_BOOT_CODE_PAGE >> 12, Lapic::Icr_low::Delivery_mode::SIPI);
	/* wait 200 us - debates ongoing whether this is still required */
	/* debates ongoing whether the second SIPI is still required */
	ipi_to_all(lapic, AP_BOOT_CODE_PAGE >> 12, Lapic::Icr_low::Delivery_mode::SIPI);

	return (unsigned)cpu_id;
}


addr_t Bios_data_area::_mmio_base_virt() { return 0x1ff000; }


Board::Serial::Serial(addr_t, size_t, unsigned baudrate)
:
	X86_uart(Bios_data_area::singleton()->serial_port(), 0, baudrate)
{ }
