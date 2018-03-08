/*
 * \brief  Platform interface implementation
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \author Alexander Boettcher
 * \date   2009-10-02
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/sleep.h>
#include <base/thread.h>
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

using namespace Genode;
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
Utcb *__main_thread_utcb;


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
addr_t Platform::_map_pages(addr_t const phys_addr, addr_t const pages,
                            bool guard_page)
{
	addr_t const size = pages << get_page_size_log2();

	/* try to reserve contiguous virtual area */
	void *core_local_ptr = 0;
	if (region_alloc()->alloc_aligned(size + (guard_page ? get_page_size() : 0),
	                                  &core_local_ptr, get_page_size_log2()).error())
		return 0;

	addr_t const core_local_addr = reinterpret_cast<addr_t>(core_local_ptr);

	int res = map_local(__main_thread_utcb, phys_addr, core_local_addr, pages,
	                    Nova::Rights(true, true, false), true);

	return res ? 0 : core_local_addr;
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

	addr_t pf_addr = utcb->qual[1];
	addr_t pf_ip   = utcb->ip;
	addr_t pf_sp   = utcb->sp;
	addr_t pf_type = utcb->qual[0];

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
	/* create echo EC */
	enum {
		GLOBAL     = false,
		EXC_BASE   = 0
	};

	addr_t ec_sel = cap_map()->insert(1);

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
	struct Type   : Bitfield<8, 8> { };
	struct Height : Bitfield<16, 24> { };
	struct Width  : Bitfield<40, 24> { };
};

/**************
 ** Platform **
 **************/

Platform::Platform() :
	_io_mem_alloc(core_mem_alloc()), _io_port_alloc(core_mem_alloc()),
	_irq_alloc(core_mem_alloc()),
	_vm_base(0x1000), _vm_size(0), _cpus(Affinity::Space(1,1))
{
	Hip  *hip  = (Hip *)__initial_sp;
	/* check for right API version */
	if (hip->api_version != 7)
		nova_die();

	/*
	 * Determine number of available CPUs
	 *
	 * XXX As of now, we assume a one-dimensional affinity space, ignoring
	 *     the y component of the affinity location. When adding support
	 *     for two-dimensional affinity spaces, look out and adjust the use of
	 *     'Platform_thread::_location' in 'platform_thread.cc'. Also look
	 *     at the 'Thread::start' function in core/thread_start.cc.
	 */
	_cpus = Affinity::Space(hip->cpus(), 1);

	/* register UTCB of main thread */
	__main_thread_utcb = (Utcb *)(__initial_sp - get_page_size());

	/* set core pd selector */
	_core_pd_sel = hip->sel_exc;

	/* create lock used by capability allocator */
	Nova::create_sm(Nova::SM_SEL_EC, core_pd_sel(), 0);

	/* locally map the whole I/O port range */
	enum { ORDER_64K = 16 };
	map_local_one_to_one(__main_thread_utcb, Io_crd(0, ORDER_64K));
	/* map BDA region, console reads IO ports at BDA_VIRT_ADDR + 0x400 */
	enum { BDA_PHY = 0x0U, BDA_VIRT = 0x1U, BDA_VIRT_ADDR = 0x1000U };
	map_local_phys_to_virt(__main_thread_utcb,
	                       Mem_crd(BDA_PHY,  0, Rights(true, false, false)),
	                       Mem_crd(BDA_VIRT, 0, Rights(true, false, false)));


	/*
	 * Now that we can access the I/O ports for comport 0, printf works...
	 */

	/*
	 * remap main utcb to default utcb address
	 * we do this that early, because Core_mem_allocator uses
	 * the main_thread_utcb very early to establish mappings
	 */
	if (map_local(__main_thread_utcb, (addr_t)__main_thread_utcb,
	              (addr_t)main_thread_utcb(), 1, Rights(true, true, false))) {
		error("could not remap utcb of main thread");
		nova_die();
	}

	/* sanity checks */
	if (hip->sel_exc + 3 > NUM_INITIAL_PT_RESERVED) {
		error("configuration error (NUM_INITIAL_PT_RESERVED)");
		nova_die();
	}

	/* init genode cpu ids based on kernel cpu ids (used for syscalls) */
	if (sizeof(map_cpu_ids) / sizeof(map_cpu_ids[0]) < hip->cpu_max()) {
		error("number of max CPUs is larger than expected - ", hip->cpu_max(),
		      " vs ", sizeof(map_cpu_ids) / sizeof(map_cpu_ids[0]));
		nova_die();
	}
	if (!hip->remap_cpu_ids(map_cpu_ids, boot_cpu())) {
		error("re-ording cpu_id failed");
		nova_die();
	}

	/* map idle SCs */
	unsigned const log2cpu = log2(hip->cpu_max());
	if ((1U << log2cpu) != hip->cpu_max()) {
		error("number of max CPUs is not of power of 2");
		nova_die();
	}

	addr_t sc_idle_base = cap_map()->insert(log2cpu + 1);
	if (sc_idle_base & ((1UL << log2cpu) - 1)) {
		error("unaligned sc_idle_base value ", Hex(sc_idle_base));
		nova_die();
	}
	if (map_local(__main_thread_utcb, Obj_crd(0, log2cpu),
	              Obj_crd(sc_idle_base, log2cpu), true))
		nova_die();

	/* test reading out idle SCs */
	bool sc_init = true;
	for (unsigned i = 0; i < hip->cpu_max(); i++) {

		if (!hip->is_cpu_enabled(i))
			continue;

		uint64_t n_time;
		uint8_t res = Nova::sc_ctrl(sc_idle_base + i, n_time);
		if (res != Nova::NOVA_OK) {
			sc_init = false;
			error(i, " ", res, " ", n_time, " - failed");
		}
	}
	if (!sc_init)
		nova_die();

	/* configure virtual address spaces */
#ifdef __x86_64__
	_vm_size = 0x7fffc0000000UL - _vm_base;
#else
	_vm_size = 0xc0000000UL - _vm_base;
#endif

	/* set up page fault handler for core - for debugging */
	addr_t const ec_echo_sel = init_core_page_fault_handler(core_pd_sel());

	if (verbose_boot_info) {
		if (hip->has_feature_vmx())
			log("Hypervisor features VMX");
		if (hip->has_feature_svm())
			log("Hypervisor features SVM");
		log("Hypervisor reports ", _cpus.width(), "x", _cpus.height(), " "
		    "CPU", _cpus.total() > 1 ? "s" : " ");
		if (!cpuid_invariant_tsc())
			warning("CPU has no invariant TSC.");

		log("CPU ID (genode->kernel:package:core:thread) remapping");
		unsigned const cpus = hip->cpus();
		for (unsigned i = 0; i < cpus; i++)
			log(" remap (", i, "->", map_cpu_ids[i], ":",
			      hip->cpu_desc_of_cpu(map_cpu_ids[i])->package, ":",
			      hip->cpu_desc_of_cpu(map_cpu_ids[i])->core, ":",
			      hip->cpu_desc_of_cpu(map_cpu_ids[i])->thread, ") ",
			      boot_cpu() == map_cpu_ids[i] ? "boot cpu" : "");
	}

	/* initialize core allocators */
	size_t const num_mem_desc = (hip->hip_length - hip->mem_desc_offset)
	                            / hip->mem_desc_size;

	if (verbose_boot_info)
		log("Hypervisor info page contains ", num_mem_desc, " memory descriptors:");

	addr_t mem_desc_base = ((addr_t)hip + hip->mem_desc_offset);

	/* define core's virtual address space */
	addr_t virt_beg = _vm_base;
	addr_t virt_end = _vm_size;
	_core_mem_alloc.virt_alloc()->add_range(virt_beg, virt_end - virt_beg);

	/* exclude core image from core's virtual address allocator */
	addr_t const core_virt_beg = trunc_page((addr_t)&_prog_img_beg);
	addr_t const core_virt_end = round_page((addr_t)&_prog_img_end);
	addr_t const binaries_beg  = trunc_page((addr_t)&_boot_modules_binaries_begin);
	addr_t const binaries_end  = round_page((addr_t)&_boot_modules_binaries_end);

	size_t const core_size     = binaries_beg - core_virt_beg;
	region_alloc()->remove_range(core_virt_beg, core_size);

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
	unmap_local(__main_thread_utcb, binaries_beg, binaries_size >> 12);

	/* preserve Bios Data Area (BDA) in core's virtual address space */
	region_alloc()->remove_range(BDA_VIRT_ADDR, 0x1000);

	/* preserve stack area in core's virtual address space */
	region_alloc()->remove_range(stack_area_virtual_base(),
	                             stack_area_virtual_size());

	/* exclude utcb of core pager thread + empty guard pages before and after */
	region_alloc()->remove_range(CORE_PAGER_UTCB_ADDR - get_page_size(),
	                             get_page_size() * 3);

	/* exclude utcb of echo thread + empty guard pages before and after */
	region_alloc()->remove_range(Echo::ECHO_UTCB_ADDR - get_page_size(),
	                             get_page_size() * 3);

	/* exclude utcb of main thread and hip + empty guard pages before and after */
	region_alloc()->remove_range((addr_t)__main_thread_utcb - get_page_size(),
	                             get_page_size() * 4);

	/* sanity checks */
	addr_t check [] = {
		reinterpret_cast<addr_t>(__main_thread_utcb), CORE_PAGER_UTCB_ADDR,
		Echo::ECHO_UTCB_ADDR, BDA_VIRT_ADDR
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

	/*
	 * All "available" ram must be added to our physical allocator before all
	 * non "available" regions that overlaps with ram get removed.
	 */
	for (unsigned i = 0; i < num_mem_desc; i++, mem_desc++) {
		if (mem_desc->type == Hip::Mem_desc::FRAMEBUFFER)
			boot_fb = mem_desc;
		if (mem_desc->type != Hip::Mem_desc::AVAILABLE_MEMORY) continue;

		if (verbose_boot_info) {
			addr_t const base = mem_desc->addr;
			size_t const size = mem_desc->size;
			log("detected physical memory: ", Hex(base, Hex::PREFIX, Hex::PAD),
			    " - size: ", Hex(size, Hex::PREFIX, Hex::PAD));
		}

		if (!mem_desc->size) continue;

		/* skip regions above 4G on 32 bit, no op on 64 bit */
		if (mem_desc->addr > ~0UL) continue;

		addr_t base = round_page(mem_desc->addr);
		size_t size;
		/* truncate size if base+size larger then natural 32/64 bit boundary */
		if (mem_desc->addr >= ~0UL - mem_desc->size + 1)
			size = trunc_page(~0UL - mem_desc->addr + 1);
		else
			size = trunc_page(mem_desc->addr + mem_desc->size) - base;

		if (verbose_boot_info)
			log("use      physical memory: ", Hex(base, Hex::PREFIX, Hex::PAD),
			    " - size: ", Hex(size, Hex::PREFIX, Hex::PAD));

		_io_mem_alloc.remove_range(base, size);
		ram_alloc()->add_range(base, size);
	}

	/*
	 * Exclude all non-available memory from physical allocator AFTER all
	 * available RAM was added - otherwise the non-available memory gets not
	 * properly removed from the physical allocator
	 */
	mem_desc = (Hip::Mem_desc *)mem_desc_base;
	for (unsigned i = 0; i < num_mem_desc; i++, mem_desc++) {
		if (mem_desc->type == Hip::Mem_desc::AVAILABLE_MEMORY) continue;

		/* skip regions above 4G on 32 bit, no op on 64 bit */
		if (mem_desc->addr > ~0UL) continue;

		addr_t base = trunc_page(mem_desc->addr);
		size_t size = mem_desc->size;

		/* remove framebuffer from available memory */
		if (mem_desc->type == Hip::Mem_desc::FRAMEBUFFER) {
			uint32_t const height = Resolution::Height::get(mem_desc->size);
			uint32_t const pitch  = mem_desc->aux;
			/* calculate size of framebuffer */
			size = pitch * height;
		}

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
			_io_mem_alloc.add_range(base, size);

		ram_alloc()->remove_range(base, size);
	}

	/* needed as I/O memory by the VESA driver */
	_io_mem_alloc.add_range(0, 0x1000);
	ram_alloc()->remove_range(0, 0x1000);

	/* exclude pages holding multi-boot command lines from core allocators */
	mem_desc = (Hip::Mem_desc *)mem_desc_base;
	addr_t prev_cmd_line_page = ~0, curr_cmd_line_page = 0;
	for (unsigned i = 0; i < num_mem_desc; i++, mem_desc++) {
		if (mem_desc->type != Hip::Mem_desc::MULTIBOOT_MODULE) continue;
		if (!mem_desc->aux) continue;

		curr_cmd_line_page = mem_desc->aux >> get_page_size_log2();
		if (curr_cmd_line_page == prev_cmd_line_page) continue;

		ram_alloc()->remove_range(curr_cmd_line_page << get_page_size_log2(),
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

		Hip::Mem_desc * mem_d = (Hip::Mem_desc *)mem_desc_base;
		for (unsigned j = 0; j < num_mem_desc; j++, mem_d++) {
			if (mem_d->type == Hip::Mem_desc::AVAILABLE_MEMORY) continue;
			if (mem_d->type == Hip::Mem_desc::ACPI_RSDT) continue;
			if (mem_d->type == Hip::Mem_desc::ACPI_XSDT) continue;
			if (mem_d->type == Hip::Mem_desc::FRAMEBUFFER) continue;
			if (mem_d == mem_desc) continue;

			/* if regions are disjunct all is fine */
			if ((mem_d->addr + mem_d->size <= mem_desc->addr) ||
			    (mem_d->addr >= mem_desc->addr + mem_desc->size))
				continue;

			error("region overlap ",
			      Hex_range<addr_t>(mem_desc->addr, mem_desc->size), " "
			      "(", (int)mem_desc->type, ") with ",
			      Hex_range<addr_t>(mem_d->addr, mem_d->size), " "
			      "(", (int)mem_d->type, ")");
			nova_die();
		}
	}

	/*
	 * From now on, it is save to use the core allocators...
	 */

	uint64_t rsdt = 0UL;
	uint64_t xsdt = 0UL;

	mem_desc = (Hip::Mem_desc *)mem_desc_base;
	for (unsigned i = 0; i < num_mem_desc; i++, mem_desc++) {
		if (mem_desc->type == Hip::Mem_desc::ACPI_RSDT) rsdt = mem_desc->addr;
		if (mem_desc->type == Hip::Mem_desc::ACPI_XSDT) xsdt = mem_desc->addr;
		if (mem_desc->type != Hip::Mem_desc::MULTIBOOT_MODULE) continue;
		if (!mem_desc->addr || !mem_desc->size) continue;

		/* assume core's ELF image has one-page header */
		_core_phys_start = trunc_page(mem_desc->addr + get_page_size());
	}

	_init_rom_modules();

	{
		/* export x86 platform specific infos */

		unsigned const pages = 1;
		void * phys_ptr = 0;
		ram_alloc()->alloc_aligned(get_page_size(), &phys_ptr, get_page_size_log2());
		addr_t const phys_addr = reinterpret_cast<addr_t>(phys_ptr);
		addr_t const core_local_addr = _map_pages(phys_addr, pages);

		Genode::Xml_generator xml(reinterpret_cast<char *>(core_local_addr),
		                          pages << get_page_size_log2(),
		                          "platform_info", [&] ()
		{
			xml.node("acpi", [&] () {

				xml.attribute("revision", 2); /* XXX */

				if (rsdt)
					xml.attribute("rsdt", String<32>(Hex(rsdt)));

				if (xsdt)
					xml.attribute("xsdt", String<32>(Hex(xsdt)));
			});
			xml.node("boot", [&] () {
				if (!boot_fb)
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
					xml.attribute("svm", hip->has_feature_svm());
					xml.attribute("vmx", hip->has_feature_vmx());
				});
				xml.node("tsc", [&] () {
					xml.attribute("invariant", cpuid_invariant_tsc());
					xml.attribute("freq_khz" , hip->tsc_freq);
				});
			});
		});

		unmap_local(__main_thread_utcb, core_local_addr, pages);
		region_alloc()->free(reinterpret_cast<void *>(core_local_addr), pages * get_page_size());

		_rom_fs.insert(new (core_mem_alloc())
		               Rom_module(phys_addr, pages * get_page_size(),
		               "platform_info"));
	}

	/* core log as ROM module */
	{
		void * phys_ptr = nullptr;
		unsigned const pages  = 1;
		size_t const log_size = pages << get_page_size_log2();

		ram_alloc()->alloc_aligned(log_size, &phys_ptr, get_page_size_log2());
		addr_t const phys_addr = reinterpret_cast<addr_t>(phys_ptr);

		addr_t const core_local_addr = _map_pages(phys_addr, pages, true);
		memset(reinterpret_cast<void *>(core_local_addr), 0, log_size);

		_rom_fs.insert(new (core_mem_alloc()) Rom_module(phys_addr, log_size,
		                                                 "core_log"));

		init_core_log( Core_log_range { core_local_addr, log_size } );
	}

	/* I/O port allocator (only meaningful for x86) */
	_io_port_alloc.add_range(0, 0x10000);

	/* IRQ allocator */
	_irq_alloc.add_range(0, hip->sel_gsi);
	_gsi_base_sel = (hip->mem_desc_offset - hip->cpu_desc_offset) / hip->cpu_desc_size;

	log(_rom_fs);

	/* add capability selector ranges to map */
	unsigned const first_index = 0x2000;
	unsigned index = first_index;
	for (unsigned i = 0; i < 32; i++)
	{
		void * phys_ptr = 0;
		ram_alloc()->alloc_aligned(get_page_size(), &phys_ptr, get_page_size_log2());

		addr_t phys_addr = reinterpret_cast<addr_t>(phys_ptr);
		addr_t core_local_addr = _map_pages(phys_addr, 1);
		
		Cap_range * range = reinterpret_cast<Cap_range *>(core_local_addr);
		construct_at<Cap_range>(range, index);

		cap_map()->insert(range);

		index = range->base() + range->elements();
	}
	_max_caps = index - first_index;

	/* add idle ECs to trace sources */
	for (unsigned genode_cpu_id = 0; genode_cpu_id < _cpus.width(); genode_cpu_id++) {

		unsigned kernel_cpu_id = Platform::kernel_cpu_id(genode_cpu_id);

		if (!hip->is_cpu_enabled(kernel_cpu_id))
			continue;

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

				enum SYSCALL_OP { IDLE_SC = 0, CROSS_SC = 1 };
				uint8_t syscall_op = (name == "cross") ? CROSS_SC : IDLE_SC;

				uint8_t res = Nova::sc_ctrl(sc_sel, sc_time, syscall_op);
				if (res != Nova::NOVA_OK)
					warning("sc_ctrl on idle SC cap, op=", syscall_op,
				            ", res=", res);

				return { Session_label("kernel"), Trace::Thread_name(name),
				         Trace::Execution_time(sc_time), affinity };
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

		new (core_mem_alloc()) Trace_source(Trace::sources(),
		                                    Affinity::Location(genode_cpu_id, 0,
		                                                       _cpus.width(), 1),
		                                    sc_idle_base + kernel_cpu_id,
		                                    "idle");

		new (core_mem_alloc()) Trace_source(Trace::sources(),
		                                    Affinity::Location(genode_cpu_id, 0,
		                                                       _cpus.width(), 1),
		                                    sc_idle_base + kernel_cpu_id,
		                                    "cross");
	}

	/* add echo thread and EC root thread to trace sources */
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
			uint64_t sc_time = 0;

			if (name == "root") {
				uint8_t res = Nova::sc_ctrl(ec_sc_sel + 1, sc_time);
				if (res != Nova::NOVA_OK)
					warning("sc_ctrl for ", name, " thread failed res=", res);
			}

			return { Session_label("core"), name,
			         Trace::Execution_time(sc_time), location };
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
		                  ec_echo_sel, "echo");

	new (core_mem_alloc())
		Core_trace_source(Trace::sources(),
		                  Affinity::Location(0, 0, _cpus.width(), 1),
		                  hip->sel_exc + 1, "root");
}


addr_t Platform::_rom_module_phys(addr_t virt)
{
	return virt - (addr_t)&_prog_img_beg + _core_phys_start;
}


unsigned Platform::kernel_cpu_id(unsigned genode_cpu_id)
{
	if (genode_cpu_id >= sizeof(map_cpu_ids) / sizeof(map_cpu_ids[0])) {
		error("invalid genode cpu id ", genode_cpu_id);
		return ~0U;
	}
	return map_cpu_ids[genode_cpu_id];
}


/****************************************
 ** Support for core memory management **
 ****************************************/

bool Mapped_mem_allocator::_map_local(addr_t virt_addr, addr_t phys_addr,
                                      unsigned size)
{
	map_local((Utcb *)Thread::myself()->utcb(), phys_addr,
	          virt_addr, size / get_page_size(),
	          Rights(true, true, false), true);
	return true;
}


bool Mapped_mem_allocator::_unmap_local(addr_t virt_addr, addr_t, unsigned size)
{
	unmap_local((Utcb *)Thread::myself()->utcb(),
	            virt_addr, size / get_page_size());
	return true;
}


/********************************
 ** Generic platform interface **
 ********************************/

void Platform::wait_for_exit() { sleep_forever(); }

