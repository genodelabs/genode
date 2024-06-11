/*
 * \brief  Platform interface implementation
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \author Alexander Boettcher
 * \date   2009-10-02
 */

/*
 * Copyright (C) 2009-2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/sleep.h>
#include <base/thread.h>
#include <util/bit_array.h>
#include <util/mmio.h>
#include <util/string.h>
#include <util/xml_generator.h>
#include <trace/source_registry.h>
#include <util/construct_at.h>

/* core includes */
#include <boot_modules.h>
#include <core_log.h>
#include <platform.h>
#include <nova_util.h>
#include <util.h>
#include <ipc_pager.h>

/* base-internal includes */
#include <base/internal/stack_area.h>
#include <base/internal/native_utcb.h>
#include <base/internal/globals.h>

/* NOVA includes */
#include <nova/syscalls.h>
#include <nova/util.h>

using namespace Core;
using namespace Nova;


enum { verbose_boot_info = true };

Native_utcb *main_thread_utcb();


/**
 * Initial value of esp register, saved by the crt0 startup code.
 * This value contains the address of the hypervisor information page.
 */
extern addr_t __initial_sp;


/**
 * Pointer to the UTCB of the main thread
 */
static Utcb *__main_thread_utcb;


/**
 * Virtual address range consumed by core's program image
 */
extern unsigned _prog_img_beg, _prog_img_end;


/**
 * Map preserved physical pages core-exclusive
 *
 * This function uses the virtual-memory region allocator to find a region
 * fitting the desired mapping. Other allocators are left alone.
 */
addr_t Core::Platform::_map_pages(addr_t const phys_addr, addr_t const pages,
                                  bool guard_page)
{
	addr_t const size = pages << get_page_size_log2();

	/* try to reserve contiguous virtual area */
	return region_alloc().alloc_aligned(size + (guard_page ? get_page_size() : 0),
	                                    get_page_size_log2()).convert<addr_t>(
		[&] (void *core_local_ptr) {

			addr_t const core_local_addr = reinterpret_cast<addr_t>(core_local_ptr);

			int res = map_local(_core_pd_sel, *__main_thread_utcb, phys_addr,
			                    core_local_addr, pages,
			                    Nova::Rights(true, true, false), true);

			return res ? 0 : core_local_addr;
		},

		[&] (Allocator::Alloc_error) {
			return 0UL; });
}


/*****************************
 ** Core page-fault handler **
 *****************************/


enum { CORE_PAGER_UTCB_ADDR = 0xbff02000 };


/**
 * IDC handler for the page-fault portal
 */
static void page_fault_handler()
{
	Utcb *utcb = (Utcb *)CORE_PAGER_UTCB_ADDR;

	addr_t  const pf_addr = utcb->pf_addr();
	addr_t  const pf_ip   = utcb->ip;
	addr_t  const pf_sp   = utcb->sp;
	uint8_t const pf_type = utcb->pf_type();

	error("\nPAGE-FAULT IN CORE addr=", Hex(pf_addr), " ip=", Hex(pf_ip),
	      " (", (pf_type & Ipc_pager::ERR_W) ? "write" : "read", ")");

	log("\nstack pointer ", Hex(pf_sp), ", qualifiers ", Hex(pf_type), " ",
	    pf_type & Ipc_pager::ERR_I ? "I" : "i",
	    pf_type & Ipc_pager::ERR_R ? "R" : "r",
	    pf_type & Ipc_pager::ERR_U ? "U" : "u",
	    pf_type & Ipc_pager::ERR_W ? "W" : "w",
	    pf_type & Ipc_pager::ERR_P ? "P" : "p");

	if ((stack_area_virtual_base() <= pf_sp) &&
		(pf_sp < stack_area_virtual_base() +
		         stack_area_virtual_size()))
	{
		addr_t utcb_addr_f  = pf_sp / stack_virtual_size();
		utcb_addr_f        *= stack_virtual_size();
		utcb_addr_f        += stack_virtual_size();
		utcb_addr_f        -= 4096;

		Nova::Utcb * utcb_fault = reinterpret_cast<Nova::Utcb *>(utcb_addr_f);
		unsigned last_items = utcb_fault->msg_items();

		log("faulter utcb ", utcb_fault, ", last message item count ", last_items);

		for (unsigned i = 0; i < last_items; i++) {
			Nova::Utcb::Item * item = utcb_fault->get_item(i);
			if (!item)
				break;

			Nova::Crd crd(item->crd);
			if (crd.is_null())
				continue;

			log(i, " - "
			    "type=",    Hex(crd.type()), " "
			    "rights=",  Hex(crd.rights()), " "
			    "region=",  Hex(crd.addr()), "+", Hex(1UL << (12 + crd.order())), " "
			    "hotspot=", Hex(crd.hotspot(item->hotspot)),
			                "(", Hex(item->hotspot), ")"
			    " - ", item->is_del() ? "delegated" : "translated");
		}
	}

	/* dump stack trace */
	struct Core_img
	{
		addr_t  _beg = 0;
		addr_t  _end = 0;
		addr_t *_ip  = nullptr;

		Core_img(addr_t sp)
		{
			extern addr_t _dtors_end;
			_beg = (addr_t)&_prog_img_beg;
			_end = (addr_t)&_dtors_end;

			_ip = (addr_t *)sp;
			for (;!ip_valid(); _ip++) {}
		}

		addr_t *ip()       { return _ip; }
		void    next_ip()  { _ip = ((addr_t *)*(_ip - 1)) + 1;}
		bool    ip_valid() { return (*_ip >= _beg) && (*_ip < _end); }
	};

	int count = 1;
	log("  #", count++, " ", Hex(pf_sp, Hex::PREFIX, Hex::PAD), " ",
	                         Hex(pf_ip, Hex::PREFIX, Hex::PAD));

	Core_img dump(pf_sp);
	while (dump.ip_valid()) {
		log("  #", count++, " ", Hex((addr_t)dump.ip(), Hex::PREFIX, Hex::PAD),
		                    " ", Hex(*dump.ip(),        Hex::PREFIX, Hex::PAD));
		dump.next_ip();
	}

	sleep_forever();
}


static addr_t core_pager_stack_top()
{
	enum { STACK_SIZE = 4*1024 };
	static char stack[STACK_SIZE];
	return (addr_t)&stack[STACK_SIZE - sizeof(addr_t)];
}


/**
 * Startup handler for core threads
 */
static void startup_handler()
{
	Utcb *utcb = (Utcb *)CORE_PAGER_UTCB_ADDR;

	/* initial IP is on stack */
	utcb->ip = *reinterpret_cast<addr_t *>(utcb->sp);
	utcb->mtd = Mtd::EIP | Mtd::ESP;
	utcb->set_msg_word(0);

	reply((void*)core_pager_stack_top());
}


static addr_t init_core_page_fault_handler(addr_t const core_pd_sel)
{
	/* create fault handler EC for core main thread */
	enum {
		GLOBAL     = false,
		EXC_BASE   = 0
	};

	addr_t ec_sel = cap_map().insert(1);

	uint8_t ret = create_ec(ec_sel, core_pd_sel, boot_cpu(),
	                        CORE_PAGER_UTCB_ADDR, core_pager_stack_top(),
	                        EXC_BASE, GLOBAL);
	if (ret)
		log(__func__, ": create_ec returned ", ret);

	/* set up page-fault portal */
	create_pt(PT_SEL_PAGE_FAULT, core_pd_sel, ec_sel,
	          Mtd(Mtd::QUAL | Mtd::ESP | Mtd::EIP),
	          (addr_t)page_fault_handler);
	revoke(Obj_crd(PT_SEL_PAGE_FAULT, 0, Obj_crd::RIGHT_PT_CTRL));

	/* startup portal for global core threads */
	create_pt(PT_SEL_STARTUP, core_pd_sel, ec_sel,
	          Mtd(Mtd::EIP | Mtd::ESP),
	          (addr_t)startup_handler);
	revoke(Obj_crd(PT_SEL_STARTUP, 0, Obj_crd::RIGHT_PT_CTRL));

	return ec_sel;
}


static bool cpuid_invariant_tsc()
{
	unsigned long cpuid = 0x80000007, edx = 0;
#ifdef __x86_64__
	asm volatile ("cpuid" : "+a" (cpuid), "=d" (edx) : : "rbx", "rcx");
#else
	asm volatile ("push %%ebx  \n"
	              "cpuid       \n"
	              "pop  %%ebx" : "+a" (cpuid), "=d" (edx) : : "ecx");
#endif
	return edx & 0x100;
}

/* boot framebuffer resolution */
struct Resolution : Register<64>
{
	struct Bpp    : Bitfield<0, 8> { };
	struct Type   : Bitfield<8, 8> { enum { VGA_TEXT = 2 }; };
	struct Height : Bitfield<16, 24> { };
	struct Width  : Bitfield<40, 24> { };
};

static Affinity::Space setup_affinity_space(Hip const &hip)
{
	unsigned cpus = 0;
	unsigned ids_thread = 0;
	Bit_array<1 << (sizeof(Hip::Cpu_desc::thread) * 8)> threads;

	hip.for_each_enabled_cpu([&](Hip::Cpu_desc const &cpu, unsigned) {
		cpus ++;
		if (threads.get(cpu.thread, 1)) return;

		threads.set(cpu.thread, 1);
		ids_thread ++;
	});

	if (ids_thread && ((cpus % ids_thread) == 0))
		return Affinity::Space(cpus / ids_thread, ids_thread);

	/* mixture of system with cores with and without hyperthreads ? */
	return Affinity::Space(cpus, 1);
}

/**************
 ** Platform **
 **************/

Core::Platform::Platform()
:
	_io_mem_alloc(&core_mem_alloc()), _io_port_alloc(&core_mem_alloc()),
	_irq_alloc(&core_mem_alloc()),
	_vm_base(0x1000), _vm_size(0), _cpus(Affinity::Space(1,1))
{
	Hip const &hip = *(Hip *)__initial_sp;
	/* check for right API version */
	if (hip.api_version != 9)
		nova_die();

	/* determine number of available CPUs */
	_cpus = setup_affinity_space(hip);

	/* register UTCB of main thread */
	__main_thread_utcb = (Utcb *)(__initial_sp - get_page_size());

	/* set core pd selector */
	_core_pd_sel = hip.sel_exc;

	/* create lock used by capability allocator */
	Nova::create_sm(Nova::SM_SEL_EC, core_pd_sel(), 0);

	/* locally map the whole I/O port range */
	enum { ORDER_64K = 16 };
	map_local_one_to_one(*__main_thread_utcb, Io_crd(0, ORDER_64K), _core_pd_sel);
	/* map BDA region, console reads IO ports at BDA_VIRT_ADDR + 0x400 */
	enum { BDA_PHY = 0x0U, BDA_VIRT = 0x1U, BDA_VIRT_ADDR = 0x1000U };
	map_local_phys_to_virt(*__main_thread_utcb,
	                       Mem_crd(BDA_PHY,  0, Rights(true, false, false)),
	                       Mem_crd(BDA_VIRT, 0, Rights(true, false, false)),
	                       _core_pd_sel);


	/*
	 * Now that we can access the I/O ports for comport 0, printf works...
	 */

	/*
	 * Mark successful boot of hypervisor for automatic tests. This must be
	 * done before core_log is initialized to prevent unexpected-reboot
	 * detection.
	 */
	log("\nHypervisor ", String<sizeof(hip.signature)+1>((char const *)&hip.signature),
	    " (API v", hip.api_version, ")");

	/*
	 * remap main utcb to default utcb address
	 * we do this that early, because Core_mem_allocator uses
	 * the main_thread_utcb very early to establish mappings
	 */
	if (map_local(_core_pd_sel, *__main_thread_utcb, (addr_t)__main_thread_utcb,
	              (addr_t)main_thread_utcb(), 1, Rights(true, true, false))) {
		error("could not remap utcb of main thread");
		nova_die();
	}

	/* sanity checks */
	if (hip.sel_exc + 3 > NUM_INITIAL_PT_RESERVED) {
		error("configuration error (NUM_INITIAL_PT_RESERVED)");
		nova_die();
	}

	/* init genode cpu ids based on kernel cpu ids (used for syscalls) */
	if (sizeof(map_cpu_ids) / sizeof(map_cpu_ids[0]) < hip.cpu_max()) {
		error("number of max CPUs is larger than expected - ", hip.cpu_max(),
		      " vs ", sizeof(map_cpu_ids) / sizeof(map_cpu_ids[0]));
		nova_die();
	}
	if (!hip.remap_cpu_ids(map_cpu_ids, (unsigned)boot_cpu())) {
		error("re-ording cpu_id failed");
		nova_die();
	}

	/* map idle SCs */
	unsigned const log2cpu = log2(hip.cpu_max());
	if ((1U << log2cpu) != hip.cpu_max()) {
		error("number of max CPUs is not of power of 2");
		nova_die();
	}

	addr_t sc_idle_base = cap_map().insert(log2cpu + 1);
	if (sc_idle_base & ((1UL << log2cpu) - 1)) {
		error("unaligned sc_idle_base value ", Hex(sc_idle_base));
		nova_die();
	}
	if (map_local(_core_pd_sel, *__main_thread_utcb, Obj_crd(0, log2cpu),
	              Obj_crd(sc_idle_base, log2cpu), true))
		nova_die();

	/* configure virtual address spaces */
#ifdef __x86_64__
	_vm_size = 0x7fffc0000000UL - _vm_base;
#else
	_vm_size = 0xc0000000UL - _vm_base;
#endif

	/* set up page fault handler for core - for debugging */
	addr_t const ec_core_exc_sel = init_core_page_fault_handler(core_pd_sel());

	/* initialize core allocators */
	size_t const num_mem_desc = (hip.hip_length - hip.mem_desc_offset)
	                            / hip.mem_desc_size;

	addr_t mem_desc_base = ((addr_t)&hip + hip.mem_desc_offset);

	/* define core's virtual address space */
	addr_t virt_beg = _vm_base;
	addr_t virt_end = _vm_size;
	_core_mem_alloc.virt_alloc().add_range(virt_beg, virt_end - virt_beg);

	/* exclude core image from core's virtual address allocator */
	addr_t const core_virt_beg = trunc_page((addr_t)&_prog_img_beg);
	addr_t const core_virt_end = round_page((addr_t)&_prog_img_end);
	addr_t const binaries_beg  = trunc_page((addr_t)&_boot_modules_binaries_begin);
	addr_t const binaries_end  = round_page((addr_t)&_boot_modules_binaries_end);

	size_t const core_size     = binaries_beg - core_virt_beg;
	region_alloc().remove_range(core_virt_beg, core_size);

	if (verbose_boot_info || binaries_end != core_virt_end) {
		log("core     image  ",
		    Hex_range<addr_t>(core_virt_beg, core_virt_end - core_virt_beg));
		log("binaries region ",
		    Hex_range<addr_t>(binaries_beg,  binaries_end - binaries_beg),
		    " free for reuse");
	}
	if (binaries_end != core_virt_end)
		nova_die();

	/* ROM modules are un-used by core - de-detach region */
	addr_t const binaries_size  = binaries_end - binaries_beg;
	unmap_local(*__main_thread_utcb, binaries_beg, binaries_size >> 12);

	/* preserve Bios Data Area (BDA) in core's virtual address space */
	region_alloc().remove_range(BDA_VIRT_ADDR, 0x1000);

	/* preserve stack area in core's virtual address space */
	region_alloc().remove_range(stack_area_virtual_base(),
	                            stack_area_virtual_size());

	/* exclude utcb of core pager thread + empty guard pages before and after */
	region_alloc().remove_range(CORE_PAGER_UTCB_ADDR - get_page_size(),
	                            get_page_size() * 3);

	/* exclude utcb of main thread and hip + empty guard pages before and after */
	region_alloc().remove_range((addr_t)__main_thread_utcb - get_page_size(),
	                            get_page_size() * 4);

	/* sanity checks */
	addr_t check [] = {
		reinterpret_cast<addr_t>(__main_thread_utcb), CORE_PAGER_UTCB_ADDR,
		BDA_VIRT_ADDR
	};

	for (unsigned i = 0; i < sizeof(check) / sizeof(check[0]); i++) { 
		if (stack_area_virtual_base() <= check[i] &&
			check[i] < stack_area_virtual_base() + stack_area_virtual_size())
		{
			error("overlapping area - ",
			      Hex_range<addr_t>(stack_area_virtual_base(),
			                        stack_area_virtual_size()), " vs ",
			      Hex(check[i]));
			nova_die();
		}
	}
 
	/* initialize core's physical-memory and I/O memory allocator */
	_io_mem_alloc.add_range(0, ~0xfffUL);
	Hip::Mem_desc *mem_desc = (Hip::Mem_desc *)mem_desc_base;

	Hip::Mem_desc *boot_fb = nullptr;

	bool efi_boot = false;
	size_t kernel_memory = 0;

	/*
	 * All "available" ram must be added to our physical allocator before all
	 * non "available" regions that overlaps with ram get removed.
	 */
	for (unsigned i = 0; i < num_mem_desc; i++, mem_desc++) {
		/* 32/64bit EFI image handle pointer - see multiboot spec 2 */
		if (mem_desc->type == 20 || mem_desc->type == 19)
			efi_boot = true;

		if (mem_desc->type == Hip::Mem_desc::FRAMEBUFFER)
			boot_fb = mem_desc;
		if (mem_desc->type == Hip::Mem_desc::MICROHYPERVISOR)
			kernel_memory += (size_t)mem_desc->size;

		if (mem_desc->type != Hip::Mem_desc::AVAILABLE_MEMORY) continue;

		if (verbose_boot_info) {
			uint64_t const base = mem_desc->addr;
			uint64_t const size = mem_desc->size;
			log("detected physical memory: ", Hex(base, Hex::PREFIX, Hex::PAD),
			    " - size: ", Hex(size, Hex::PREFIX, Hex::PAD));
		}

		if (!mem_desc->size) continue;

		/* skip regions above 4G on 32 bit, no op on 64 bit */
		if (mem_desc->addr > ~0UL) continue;

		uint64_t base = round_page(mem_desc->addr);
		uint64_t size;
		/* truncate size if base+size larger then natural 32/64 bit boundary */
		if (mem_desc->addr >= ~0ULL - mem_desc->size + 1)
			size = trunc_page(~0ULL - mem_desc->addr + 1);
		else
			size = trunc_page(mem_desc->addr + mem_desc->size) - base;

		if (verbose_boot_info)
			log("use      physical memory: ", Hex(base, Hex::PREFIX, Hex::PAD),
			    " - size: ", Hex(size, Hex::PREFIX, Hex::PAD));

		_io_mem_alloc.remove_range((addr_t)base, (size_t)size);
		ram_alloc().add_range((addr_t)base, (size_t)size);
	}

	addr_t hyp_log = 0;
	size_t hyp_log_size = 0;

	/*
	 * Exclude all non-available memory from physical allocator AFTER all
	 * available RAM was added - otherwise the non-available memory gets not
	 * properly removed from the physical allocator
	 */
	mem_desc = (Hip::Mem_desc *)mem_desc_base;
	for (unsigned i = 0; i < num_mem_desc; i++, mem_desc++) {
		if (mem_desc->type == Hip::Mem_desc::AVAILABLE_MEMORY) continue;

		if (mem_desc->type == Hip::Mem_desc::HYPERVISOR_LOG) {
			hyp_log      = (addr_t)mem_desc->addr;
			hyp_log_size = (size_t)mem_desc->size;
		}

		uint64_t base = trunc_page(mem_desc->addr);
		uint64_t size = mem_desc->size;

		/* remove framebuffer from available memory */
		if (mem_desc->type == Hip::Mem_desc::FRAMEBUFFER) {
			uint32_t const height = (uint32_t)Resolution::Height::get(mem_desc->size);
			uint32_t const pitch  = mem_desc->aux;
			/* calculate size of framebuffer */
			size = pitch * height;
		}

		if (verbose_boot_info)
			log("reserved memory: ", Hex(mem_desc->addr), " - size: ",
			    Hex(size), " type=", (int)mem_desc->type);

		/* skip regions above 4G on 32 bit, no op on 64 bit */
		if (mem_desc->addr > ~0UL) continue;

		/* truncate size if base+size larger then natural 32/64 bit boundary */
		if (mem_desc->addr + size < mem_desc->addr)
			size = 0UL - base;
		else
			size = round_page(mem_desc->addr + size) - base;

		if (!size)
			continue;

		/* make acpi regions as io_mem available to platform driver */
		if (mem_desc->type == Hip::Mem_desc::ACPI_RECLAIM_MEMORY ||
		    mem_desc->type == Hip::Mem_desc::ACPI_NVS_MEMORY)
			_io_mem_alloc.add_range((addr_t)base, (size_t)size);

		ram_alloc().remove_range((addr_t)base, (size_t)size);
	}

	/* needed as I/O memory by the VESA driver */
	_io_mem_alloc.add_range(0, 0x1000);
	ram_alloc().remove_range(0, 0x1000);

	/* exclude pages holding multi-boot command lines from core allocators */
	mem_desc = (Hip::Mem_desc *)mem_desc_base;
	addr_t prev_cmd_line_page = ~0, curr_cmd_line_page = 0;
	for (unsigned i = 0; i < num_mem_desc; i++, mem_desc++) {
		if (mem_desc->type != Hip::Mem_desc::MULTIBOOT_MODULE) continue;
		if (!mem_desc->aux) continue;

		curr_cmd_line_page = mem_desc->aux >> get_page_size_log2();
		if (curr_cmd_line_page == prev_cmd_line_page) continue;

		ram_alloc().remove_range(curr_cmd_line_page << get_page_size_log2(),
		                         get_page_size() * 2);
		prev_cmd_line_page = curr_cmd_line_page;
	}

	/* sanity checks that regions don't overlap - could be bootloader issue */
	mem_desc = (Hip::Mem_desc *)mem_desc_base;
	for (unsigned i = 0; i < num_mem_desc; i++, mem_desc++) {

		if (mem_desc->type == Hip::Mem_desc::AVAILABLE_MEMORY) continue;
		if (mem_desc->type == Hip::Mem_desc::ACPI_RSDT) continue;
		if (mem_desc->type == Hip::Mem_desc::ACPI_XSDT) continue;
		if (mem_desc->type == Hip::Mem_desc::FRAMEBUFFER) continue;
		if (mem_desc->type == Hip::Mem_desc::EFI_SYSTEM_TABLE) continue;

		Hip::Mem_desc * mem_d = (Hip::Mem_desc *)mem_desc_base;
		for (unsigned j = 0; j < num_mem_desc; j++, mem_d++) {
			if (mem_d->type == Hip::Mem_desc::AVAILABLE_MEMORY) continue;
			if (mem_d->type == Hip::Mem_desc::ACPI_RSDT) continue;
			if (mem_d->type == Hip::Mem_desc::ACPI_XSDT) continue;
			if (mem_d->type == Hip::Mem_desc::FRAMEBUFFER) continue;
			if (mem_d->type == Hip::Mem_desc::EFI_SYSTEM_TABLE) continue;
			if (mem_d == mem_desc) continue;

			/* if regions are disjunct all is fine */
			if ((mem_d->addr + mem_d->size <= mem_desc->addr) ||
			    (mem_d->addr >= mem_desc->addr + mem_desc->size))
				continue;

			error("region overlap ",
			      Hex_range<addr_t>((addr_t)mem_desc->addr, (size_t)mem_desc->size), " "
			      "(", (int)mem_desc->type, ") with ",
			      Hex_range<addr_t>((addr_t)mem_d->addr, (size_t)mem_d->size), " "
			      "(", (int)mem_d->type, ")");
			nova_die();
		}
	}

	/*
	 * From now on, it is save to use the core allocators...
	 */

	uint64_t efi_sys_tab_phy = 0UL;
	uint64_t rsdt = 0UL;
	uint64_t xsdt = 0UL;

	mem_desc = (Hip::Mem_desc *)mem_desc_base;
	for (unsigned i = 0; i < num_mem_desc; i++, mem_desc++) {
		if (mem_desc->type == Hip::Mem_desc::EFI_SYSTEM_TABLE) efi_sys_tab_phy = mem_desc->addr;
		if (mem_desc->type == Hip::Mem_desc::ACPI_RSDT) rsdt = mem_desc->addr;
		if (mem_desc->type == Hip::Mem_desc::ACPI_XSDT) xsdt = mem_desc->addr;
		if (mem_desc->type != Hip::Mem_desc::MULTIBOOT_MODULE) continue;
		if (!mem_desc->addr || !mem_desc->size) continue;

		/* assume core's ELF image has one-page header */
		_core_phys_start = (addr_t)trunc_page(mem_desc->addr + get_page_size());
	}

	_init_rom_modules();

	auto export_pages_as_rom_module = [&] (auto rom_name, size_t pages, auto content_fn)
	{
		size_t const bytes = pages << get_page_size_log2();
		ram_alloc().alloc_aligned(bytes, get_page_size_log2()).with_result(

			[&] (void *phys_ptr) {

				addr_t const phys_addr = reinterpret_cast<addr_t>(phys_ptr);
				char * const core_local_ptr = (char *)_map_pages(phys_addr, pages);

				if (!core_local_ptr) {
					warning("failed to export ", rom_name, " as ROM module");
					ram_alloc().free(phys_ptr, bytes);
					return;
				}

				memset(core_local_ptr, 0, bytes);
				content_fn(core_local_ptr, bytes);

				new (core_mem_alloc())
					Rom_module(_rom_fs, rom_name, phys_addr, bytes);

				/* leave the ROM backing store mapped within core */
			},

			[&] (Range_allocator::Alloc_error) {
				warning("failed to allocate physical memory for exporting ",
				        rom_name, " as ROM module"); });
	};

	export_pages_as_rom_module("platform_info", 1 + (MAX_SUPPORTED_CPUS / 32),
		[&] (char * const ptr, size_t const size) {
			Xml_generator xml(ptr, size, "platform_info", [&] ()
			{
				xml.node("kernel", [&] () {
					xml.attribute("name", "nova");
					xml.attribute("acpi", true);
					xml.attribute("msi" , true);
					xml.attribute("iommu", hip.has_feature_iommu());
				});
				if (efi_sys_tab_phy) {
					xml.node("efi-system-table", [&] () {
						xml.attribute("address", String<32>(Hex(efi_sys_tab_phy)));
					});
				}
				xml.node("acpi", [&] () {

					xml.attribute("revision", 2); /* XXX */

					if (rsdt)
						xml.attribute("rsdt", String<32>(Hex(rsdt)));

					if (xsdt)
						xml.attribute("xsdt", String<32>(Hex(xsdt)));
				});
				xml.node("affinity-space", [&] () {
					xml.attribute("width", _cpus.width());
					xml.attribute("height", _cpus.height());
				});
				xml.node("boot", [&] () {
					if (!boot_fb)
						return;

					if (!efi_boot && (Resolution::Type::get(boot_fb->size) != Resolution::Type::VGA_TEXT))
						return;

					xml.node("framebuffer", [&] () {
						xml.attribute("phys",   String<32>(Hex(boot_fb->addr)));
						xml.attribute("width",  Resolution::Width::get(boot_fb->size));
						xml.attribute("height", Resolution::Height::get(boot_fb->size));
						xml.attribute("bpp",    Resolution::Bpp::get(boot_fb->size));
						xml.attribute("type",   Resolution::Type::get(boot_fb->size));
						xml.attribute("pitch",  boot_fb->aux);
					});
				});
				xml.node("hardware", [&] () {
					xml.node("features", [&] () {
						xml.attribute("svm", hip.has_feature_svm());
						xml.attribute("vmx", hip.has_feature_vmx());
					});
					xml.node("tsc", [&] () {
						xml.attribute("invariant", cpuid_invariant_tsc());
						xml.attribute("freq_khz" , hip.tsc_freq);
					});
					xml.node("cpus", [&] () {
						for_each_location([&](Affinity::Location &location) {
							unsigned const kernel_cpu_id = Platform::kernel_cpu_id(location);
							auto const cpu_ptr = hip.cpu_desc_of_cpu(kernel_cpu_id);

							if (!cpu_ptr)
								return;

							auto const &cpu = *cpu_ptr;

							xml.node("cpu", [&] () {
								xml.attribute("xpos",     location.xpos());
								xml.attribute("ypos",     location.ypos());
								xml.attribute("id",       kernel_cpu_id);
								xml.attribute("package",  cpu.package);
								xml.attribute("core",     cpu.core);
								xml.attribute("thread",   cpu.thread);
								xml.attribute("family",   String<5>(Hex(cpu.family)));
								xml.attribute("model",    String<5>(Hex(cpu.model)));
								xml.attribute("stepping", String<5>(Hex(cpu.stepping)));
								xml.attribute("platform", String<5>(Hex(cpu.platform)));
								xml.attribute("patch",    String<12>(Hex(cpu.patch)));
								if (cpu.p_core()) xml.attribute("cpu_type", "P");
								if (cpu.e_core()) xml.attribute("cpu_type", "E");
							});
						});
					});
				});
			});
		}
	);

	export_pages_as_rom_module("core_log", 4,
		[&] (char * const ptr, size_t const size) {
			init_core_log( Core_log_range { (addr_t)ptr, size } );
	});

	/* export hypervisor log memory */
	if (hyp_log && hyp_log_size)
		new (core_mem_alloc())
			Rom_module(_rom_fs, "kernel_log", hyp_log, hyp_log_size);

	if (verbose_boot_info) {
		if (hip.has_feature_iommu())
			log("Hypervisor features IOMMU");
		if (hip.has_feature_vmx())
			log("Hypervisor features VMX");
		if (hip.has_feature_svm())
			log("Hypervisor features SVM");
		log("Hypervisor reports ", _cpus.width(), "x", _cpus.height(), " "
		    "CPU", _cpus.total() > 1 ? "s" : " ");
		if (!cpuid_invariant_tsc())
			warning("CPU has no invariant TSC.");

		log("mapping: affinity space -> kernel cpu id - package:core:thread");

		for_each_location([&](Affinity::Location &location) {
			unsigned const kernel_cpu_id = Platform::kernel_cpu_id(location);
			Hip::Cpu_desc const * cpu = hip.cpu_desc_of_cpu(kernel_cpu_id);

			Genode::String<16> text ("failure");
			if (cpu)
				text = Genode::String<16>(cpu->package, ":",
				                          cpu->core, ":", cpu->thread,
				                          cpu->e_core() ? " E" :
				                          cpu->p_core() ? " P" : "");

			log(" remap (", location.xpos(), "x", location.ypos(),") -> ",
			    kernel_cpu_id, " - ", text,
			    boot_cpu() == kernel_cpu_id ? " boot cpu" : "");
		});
	}

	/* I/O port allocator (only meaningful for x86) */
	_io_port_alloc.add_range(0, 0x10000);

	/* IRQ allocator */
	_irq_alloc.add_range(0, hip.sel_gsi);
	_gsi_base_sel = (hip.mem_desc_offset - hip.cpu_desc_offset) / hip.cpu_desc_size;

	log(_rom_fs);

	log(Number_of_bytes(kernel_memory), " kernel memory"); log("");

	/* add capability selector ranges to map */
	unsigned const first_index = 0x2000;
	unsigned index = first_index;
	for (unsigned i = 0; i < 32; i++)
	{
		void * phys_ptr = nullptr;

		ram_alloc().alloc_aligned(get_page_size(), get_page_size_log2()).with_result(
			[&] (void *ptr) { phys_ptr = ptr; },
			[&] (Range_allocator::Alloc_error) { /* covered by nullptr test below */ });

		if (phys_ptr == nullptr)
			break;

		addr_t phys_addr = reinterpret_cast<addr_t>(phys_ptr);
		addr_t core_local_addr = _map_pages(phys_addr, 1);

		if (!core_local_addr) {
			ram_alloc().free(phys_ptr);
			break;
		}

		Cap_range &range = *reinterpret_cast<Cap_range *>(core_local_addr);
		construct_at<Cap_range>(&range, index);

		cap_map().insert(range);

		index = (unsigned)(range.base() + range.elements());
	}
	_max_caps = index - first_index;

	/* add idle ECs to trace sources */
	for_each_location([&](Affinity::Location &location) {
		unsigned const kernel_cpu_id = Platform::kernel_cpu_id(location);
		if (!hip.cpu_desc_of_cpu(kernel_cpu_id)) return;

		struct Trace_source : public  Trace::Source::Info_accessor,
		                      private Trace::Control,
		                      private Trace::Source
		{
			Affinity::Location const affinity;
			unsigned           const sc_sel;
			Genode::String<8>  const name;

			/**
			 * Trace::Source::Info_accessor interface
			 */
			Info trace_source_info() const override
			{
				uint64_t sc_time = 0;
				uint64_t ec_time = 0;
				uint8_t  res = 0;

				if (name == "killed") {
					res = Nova::sc_ec_time(sc_sel, sc_sel,
					                       sc_time, ec_time);
				} else {
					auto syscall_op = (name == "cross")
					                ? Sc_op::SC_TIME_CROSS
					                : Sc_op::SC_TIME_IDLE;

					res = Nova::sc_ctrl(sc_sel, sc_time,
					                    syscall_op);

					if (syscall_op == Sc_op::SC_TIME_IDLE)
						ec_time = sc_time;
				}

				if (res != Nova::NOVA_OK)
					warning("sc_ctrl on ", name, " failed"
					        ", res=", res);

				return { Session_label("kernel"), Trace::Thread_name(name),
				         Trace::Execution_time(ec_time, sc_time), affinity };
			}

			Trace_source(Trace::Source_registry &registry,
			             Affinity::Location const affinity,
			             unsigned const sc_sel,
			             char const * type_name)
			:
				Trace::Control(),
				Trace::Source(*this, *this), affinity(affinity),
				sc_sel(sc_sel), name(type_name)
			{
				registry.insert(this);
			}
		};

		new (core_mem_alloc()) Trace_source(Trace::sources(), location,
		                                    (unsigned)(sc_idle_base + kernel_cpu_id),
		                                    "idle");

		new (core_mem_alloc()) Trace_source(Trace::sources(), location,
		                                    (unsigned)(sc_idle_base + kernel_cpu_id),
		                                    "cross");

		new (core_mem_alloc()) Trace_source(Trace::sources(), location,
		                                    (unsigned)(sc_idle_base + kernel_cpu_id),
		                                    "killed");
	});

	/* add exception handler EC for core and EC root thread to trace sources */
	struct Core_trace_source : public  Trace::Source::Info_accessor,
	                           private Trace::Control,
	                           private Trace::Source
	{
		Affinity::Location const location;
		addr_t const ec_sc_sel;
		Genode::String<8>  const name;

		/**
		 * Trace::Source::Info_accessor interface
		 */
		Info trace_source_info() const override
		{
			uint64_t ec_time = 0;
			uint64_t sc_time = 0;

			if (name == "root") {
				uint8_t res = Nova::sc_ec_time(ec_sc_sel + 1,
				                               ec_sc_sel,
				                               sc_time,
				                               ec_time);
				if (res != Nova::NOVA_OK)
					warning("sc_ec_time for root failed "
					        "res=", res);
			} else {
				uint8_t res = Nova::ec_time(ec_sc_sel, ec_time);
				if (res != Nova::NOVA_OK)
					warning("ec_time for", name, " thread "
					        "failed res=", res);
			}

			return { Session_label("core"), name,
			         Trace::Execution_time(ec_time, sc_time), location };
		}

		Core_trace_source(Trace::Source_registry &registry,
		                  Affinity::Location loc, addr_t sel,
		                  char const *name)
		:
			Trace::Control(),
			Trace::Source(*this, *this), location(loc), ec_sc_sel(sel),
			name(name)
		{
			registry.insert(this);
		}
	};

	new (core_mem_alloc())
		Core_trace_source(Trace::sources(),
		                  Affinity::Location(0, 0, _cpus.width(), 1),
		                  ec_core_exc_sel, "core_fault");

	new (core_mem_alloc())
		Core_trace_source(Trace::sources(),
		                  Affinity::Location(0, 0, _cpus.width(), 1),
		                  hip.sel_exc + 1, "root");
}


addr_t Core::Platform::_rom_module_phys(addr_t virt)
{
	return virt - (addr_t)&_prog_img_beg + _core_phys_start;
}


unsigned Core::Platform::kernel_cpu_id(Affinity::Location location) const
{
	unsigned const cpu_id = pager_index(location);

	if (cpu_id >= sizeof(map_cpu_ids) / sizeof(map_cpu_ids[0])) {
		error("invalid genode cpu id ", cpu_id);
		return ~0U;
	}

	return map_cpu_ids[cpu_id];
}


unsigned Core::Platform::pager_index(Affinity::Location location) const
{
	return (location.xpos() * _cpus.height() + location.ypos())
	       % (_cpus.width() * _cpus.height());
}


/****************************************
 ** Support for core memory management **
 ****************************************/

bool Mapped_mem_allocator::_map_local(addr_t virt_addr, addr_t phys_addr, size_t size)
{
	/* platform_specific()->core_pd_sel() deadlocks if called from platform constructor */
	Hip const &hip  = *(Hip const *)__initial_sp;
	addr_t const core_pd_sel = hip.sel_exc;

	map_local(core_pd_sel,
	          *(Utcb *)Thread::myself()->utcb(), phys_addr,
	          virt_addr, size / get_page_size(),
	          Rights(true, true, false), true);
	return true;
}


bool Mapped_mem_allocator::_unmap_local(addr_t virt_addr, addr_t, size_t size)
{
	unmap_local(*(Utcb *)Thread::myself()->utcb(),
	            virt_addr, size / get_page_size());
	return true;
}


/********************************
 ** Generic platform interface **
 ********************************/

void Core::Platform::wait_for_exit() { sleep_forever(); }

