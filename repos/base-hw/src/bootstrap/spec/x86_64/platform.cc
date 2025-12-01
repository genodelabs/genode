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

/* bootstrap includes */
#include <assert.h>
#include <bios_data_area.h>
#include <platform.h>
#include <multiboot.h>
#include <multiboot2.h>
#include <port_io.h>

#include <hw/memory_consts.h>
#include <hw/spec/x86_64/acpi.h>
#include <hw/spec/x86_64/apic.h>
#include <hw/spec/x86_64/x86_64.h>

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

static addr_t rsdp_addr = 0;


static void disable_pit()
{
	using Hw::outb;

	enum {
		/* PIT constants */
		PIT_CH0_DATA   = 0x40,
		PIT_MODE       = 0x43,
	};

	/*
	 * Disable PIT timer channel. This is necessary since BIOS sets up
	 * channel 0 to fire periodically.
	 */
	outb(PIT_MODE, 0x30);
	outb(PIT_CH0_DATA, 0);
	outb(PIT_CH0_DATA, 0);
}


/*
 * Enable dispatch serializing lfence instruction on AMD processors
 *
 * See Software techniques for managing speculation on AMD processors
 *     Revision 5.09.23
 *     Mitigation G-2
 */
static void amd_enable_serializing_lfence()
{
	using Cpu = Hw::X86_64_cpu;

	if (Hw::Vendor::get_vendor_id() != Hw::Vendor::Vendor_id::AMD)
		return;

	unsigned const family = Hw::Vendor::get_family();

	/*
	 * In family 0Fh and 11h, lfence is always dispatch serializing and
	 * "AMD plans support for this MSR and access to this bit for all future
	 * processors." from family 14h on.
	 */
	if ((family == 0x10) || (family == 0x12) || (family >= 0x14)) {
		Cpu::Amd_lfence::access_t amd_lfence = Cpu::Amd_lfence::read();
		Cpu::Amd_lfence::Enable_dispatch_serializing::set(amd_lfence);
		Cpu::Amd_lfence::write(amd_lfence);
	}
}


static inline void memory_add_region(addr_t base, addr_t size,
                                     Hw::Memory_region_array &early,
                                     Hw::Memory_region_array &late)
{
	using namespace Hw;

	static constexpr size_t initial_map_max = 1024 * 1024 * 1024;

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
			early.add(Memory_region { base, AP_BOOT_CODE_PAGE - base });

		size -= AP_BOOT_CODE_PAGE - base;
		size -= (get_page_size() > size) ? size : get_page_size();
		base  = AP_BOOT_CODE_PAGE + get_page_size();
	}

	/* skip partial 4k pages (seen with Qemu with ahci model enabled) */
	if (!aligned(base, AT_PAGE)) {
		auto new_base = align_addr(base, AT_PAGE);
		size -= (new_base - base > size) ? size : new_base - base;
		base  = new_base;
	}

	/* remove partial 4k pages */
	if (!aligned(size, AT_PAGE))
		size -= size & 0xffful;

	if (!size)
		return;

	if (base >= initial_map_max) {
		late.add(Memory_region { base, size });
		return;
	}

	if (base + size <= initial_map_max) {
		early.add(Memory_region { base, size });
		return;
	}

	size_t low_size = initial_map_max - base;
	early.add(Memory_region { base, low_size });
	late.add(Memory_region { initial_map_max, size - low_size });
}


static inline void parse_multi_boot_1(Hw::Memory_region_array &early,
                                      Hw::Memory_region_array &late)
{
	for (unsigned i = 0; true; i++) {
		using Mmap = Multiboot_info::Mmap;

		Mmap v(Multiboot_info(__initial_bx).phys_ram_mmap_base(i));
		if (!v.base()) break;

		Mmap::Addr::access_t   base = v.read<Mmap::Addr>();
		Mmap::Length::access_t size = v.read<Mmap::Length>();

		memory_add_region(base, size, early, late);
	}

	/* search ACPI RSDP pointer at known places */

	/* BIOS range to scan for */
	enum { BIOS_BASE = 0xe0000, BIOS_SIZE = 0x20000 };
	Hw::Acpi::Rsdp::search(BIOS_BASE, BIOS_SIZE,
		[&] (Hw::Acpi::Rsdp &rsdp) { rsdp_addr = rsdp.base(); },
		[&] () {
			/* page 0 is remapped to 2M - 4k - see crt_translation table */
			addr_t const bios_addr = 2 * 1024 * 1024 - 4096;

			/* search EBDA (BIOS addr + 0x40e) */
			addr_t ebda_phys = (*reinterpret_cast<uint16_t *>(bios_addr + 0x40e)) << 4;
			if (ebda_phys < 0x1000) ebda_phys = bios_addr;

			Hw::Acpi::Rsdp::search(ebda_phys, 0x1000,
				[&] (Hw::Acpi::Rsdp &rsdp) { rsdp_addr = rsdp.base(); },
				[] () { /* ignore non-result */ });
		});
}


static inline void parse_multi_boot_2(::Board::Boot_info      &info,
                                      Hw::Memory_region_array &early,
                                      Hw::Memory_region_array &late)
{
	Multiboot2_info mbi2(__initial_bx);

	mbi2.for_each_tag([&] (Multiboot2_info::Memory const &m) {
		uint32_t const type = m.read<Multiboot2_info::Memory::Type>();

		if (type != Multiboot2_info::Memory::Type::MEMORY)
			return;

		uint64_t const base = m.read<Multiboot2_info::Memory::Addr>();
		uint64_t const size = m.read<Multiboot2_info::Memory::Size>();

		memory_add_region(base, size, early, late);
	},
	[&] (Hw::Acpi::Rsdp const &rsdp_v1) {
		/* only use ACPI RSDP v1 if nothing available/valid by now */
		if (!rsdp_addr) rsdp_addr = rsdp_v1.base();
	},
	[&] (Hw::Acpi::Rsdp const &rsdp_v2) {
		/* prefer v2 ever, override stored previous rsdp v1 potentially */
		rsdp_addr = rsdp_v2.base();
	},
	[&] (Hw::Framebuffer const &fb) {
		info.framebuffer = fb;
	},
	[&] (uint64_t const efi_sys_tab) {
		info.efi_system_table = efi_sys_tab;
	});
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
	switch (__initial_ax) {
	case Multiboot_info::MAGIC:
		parse_multi_boot_1(early_ram_regions, late_ram_regions);
		break;
	case Multiboot2_info::MAGIC:
		parse_multi_boot_2(info, early_ram_regions, late_ram_regions);
		break;
	default:
		error("invalid multiboot magic value: ", Hex(__initial_ax));
	};

	if (rsdp_addr) {
		Hw::Acpi::Rsdp rsdp(rsdp_addr);

		/*
		 * Copy relevant information to boot_info to export it
		 * via platform_info from core to acpi driver, acpica
		 */
		info.acpi_revision = rsdp.revision();
		info.acpi_rsdt     = rsdp.rsdt();
		info.acpi_xsdt     = rsdp.xsdt();

		rsdp.for_each_entry(

			/* Handle FADT & FACS */
			[&] (Hw::Acpi::Fadt &fadt) {
				info.acpi_fadt = fadt.base();
				fadt.takeover_acpi();

				Hw::Acpi::Facs facs(fadt.facs());
				facs.wakeup_vector(AP_BOOT_CODE_PAGE);

				/* Add FADT to core mapped memory */
				auto mem_aligned = fadt.base() & _align_mask<addr_t>(AT_PAGE);
				auto mem_size    = align_addr(fadt.base() + fadt.size(), AT_PAGE)
				                   - mem_aligned;
				core_mmio.add({ mem_aligned, mem_size });

				/*
				 * Enable serializing lfence on supported AMD processors
				 *
				 * For all cpus this will be set up later. The boot cpu needs
				 * to do it already here to obtain the most acurate results
				 * when calibrating the TSC frequency.
				 */
				amd_enable_serializing_lfence();
				info.tsc_freq_khz = fadt.calibrate_freq_khz(10, []() {
					return Hw::Tsc::rdtsc(); });

				Hw::Apic apic(Hw::Cpu_memory_map::lapic_phys_base());
				auto result = apic.timer_calibrate(fadt);
				info.apic_freq_khz = result.freq_khz;
				info.apic_div = result.div;
			},

			/* Handle MADT */
			[&] (Hw::Acpi::Madt &madt) {
				cpus = 0;
				madt.for_each_apic(
					[&] (Hw::Acpi::Madt::Local_apic &) { cpus++; },
					[&] (Hw::Acpi::Madt::Io_apic &)    {},
					[&] (Hw::Acpi::Madt::X2_apic &)    { cpus++; }
				);
			}
		);
	};

	disable_pit();

	/* copy 16 bit boot code for AP CPUs and for ACPI resume */
	addr_t ap_code_size = (addr_t)&_start - (addr_t)&_ap;
	memcpy((void *)AP_BOOT_CODE_PAGE, &_ap, ap_code_size);


	/************************
	 ** Some sanity checks **
	 ************************/

	if (!info.apic_freq_khz && !info.apic_div) {
		info.apic_freq_khz = TIMER_MIN_TICKS_PER_MS;
		info.apic_div = 1;
		warning("Calibration failed, set minimum Local APIC frequency of ",
		        info.apic_freq_khz, "kHz");
	}

	if (!info.tsc_freq_khz) {
		uint32_t const default_freq = 2'400'000;
		warning("Calibration failed, using TSC frequency of ", default_freq, "kHz");
		info.tsc_freq_khz = default_freq;
	}

	static constexpr unsigned MAX_CPUS = Hw::Mm::CPU_LOCAL_MEMORY_AREA_SIZE /
	                                     Hw::Mm::CPU_LOCAL_MEMORY_SLOT_SIZE;
	if (!cpus || cpus > MAX_CPUS) {
		Genode::warning("CPU count is unsupported ", cpus, "/", MAX_CPUS,
		                " - invalid or missing RSDT/XSDT?!");
		cpus = !cpus ? 1 : MAX_CPUS;
	}
}


static inline void wake_up_all_cpus(Hw::Apic &apic)
{
	/* reset assembly counter (crt0.s), required for resume */
	__cpus_booted = 0;

	/* see Intel Multiprocessor documentation - we need to do INIT-SIPI-SIPI */
	apic.send_ipi_to_all(0 /* unused */,
	                     Hw::Apic::Icr_low::Delivery_mode::INIT);
	/* wait 10  ms - debates ongoing whether this is still required */
	apic.send_ipi_to_all(AP_BOOT_CODE_PAGE >> 12,
	                     Hw::Apic::Icr_low::Delivery_mode::STARTUP);
	/* wait 200 us - debates ongoing whether this is still required */
	/* debates ongoing whether the second SIPI is still required */
	apic.send_ipi_to_all(AP_BOOT_CODE_PAGE >> 12,
	                     Hw::Apic::Icr_low::Delivery_mode::STARTUP);
}


Bootstrap::Platform::Cpu_id Bootstrap::Platform::enable_mmu()
{
	using ::Board::Cpu;
	auto const cpu_id =
		Cpu_id(Cpu::Cpuid_1_ebx::Apic_id::get(Cpu::Cpuid_1_ebx::read()));
	static auto const boot_cpu_id = cpu_id;
	bool boot_cpu = boot_cpu_id == cpu_id;

	Hw::Apic apic(Hw::Cpu_memory_map::lapic_phys_base());
	if (boot_cpu && board.cpus > 1) wake_up_all_cpus(apic);

	/* enable serializing lfence on supported AMD processors. */
	amd_enable_serializing_lfence();

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

	return cpu_id;
}


addr_t Bios_data_area::_mmio_base_virt() { return 0x1ff000; }


Board::Serial::Serial(addr_t, size_t, unsigned baudrate)
:
	X86_uart(Bios_data_area::singleton()->serial_port(), 0, baudrate)
{ }


unsigned Bootstrap::Platform::_prepare_cpu_memory_area()
{
	if (!rsdp_addr)
		return 1; /* return boot cpu only */

	Hw::Acpi::Rsdp rsdp(rsdp_addr);
	rsdp.for_each_entry(
		[&] (Hw::Acpi::Fadt &) { /* ignore */ },

		[&] (Hw::Acpi::Madt &madt) {
			madt.for_each_apic(
				[&] (Hw::Acpi::Madt::Local_apic &lapic) {
					_prepare_cpu_memory_area(lapic.id());
				},
				[&] (Hw::Acpi::Madt::Io_apic &) {},
				[&] (Hw::Acpi::Madt::X2_apic &x2_apic) {
					ASSERT(x2_apic.id() < 0x100);
					_prepare_cpu_memory_area(x2_apic.id() & 0xff);
				}
			);
		});

	return board.cpus;
}
