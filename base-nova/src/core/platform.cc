/*
 * \brief  Platform interface implementation
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \author Alexander Boettcher
 * \date   2009-10-02
 */

/*
 * Copyright (C) 2009-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <base/sleep.h>
#include <base/thread.h>
#include <base/cap_sel_alloc.h>

/* core includes */
#include <core_parent.h>
#include <platform.h>
#include <nova_util.h>
#include <util.h>

/* NOVA includes */
#include <nova/syscalls.h>

using namespace Genode;
using namespace Nova;


enum { verbose_boot_info = true };

Native_utcb *main_thread_utcb();

/**
 * Initial value of esp register, saved by the crt0 startup code
 *
 * This value contains the address of the hypervisor information page.
 */
extern long __initial_sp;


/**
 * Pointer to the UTCB of the main thread
 */
Utcb *__main_thread_utcb;


/**
 * Virtual address range consumed by core's program image
 */
extern unsigned _prog_img_beg, _prog_img_end;


/**
 *  Capability selector of root PD
 */
addr_t __core_pd_sel;


/**
 * Map preserved physical page for the exclusive read-execute-only used by core
 */
addr_t Platform::_map_page(addr_t phys_page, addr_t pages)
{
	/* set an invalid address */
	void * core_local_ptr = 0;
	if (!region_alloc()->alloc(pages << get_page_size_log2(),
	                           &core_local_ptr))
		return 0;

	addr_t core_local_addr = reinterpret_cast<addr_t>(core_local_ptr);
	int res = map_local(__main_thread_utcb, phys_page << get_page_size_log2(),
	                    core_local_addr, pages,
	                    Nova::Rights(true, false, true), true);
	if (res)
		PERR("map_local failed res=%d", res);

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
	addr_t pf_ip  = utcb->ip;
	addr_t pf_sp  = utcb->sp;

	printf("\nPAGE-FAULT IN CORE: ADDR %lx  IP %lx  SP %lx stack trace follows...\n",
	       pf_addr, pf_ip, pf_sp);

	/* dump stack trace */
	struct Core_img
	{
		addr_t  _beg;
		addr_t  _end;
		addr_t *_ip;

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
		bool    ip_valid() { return *_ip >= _beg && *_ip < _end; }
	};

	int count = 1;
	printf("  #%d %08lx %08lx\n", count++, pf_sp, pf_ip);

	Core_img dump(pf_sp);
	while (dump.ip_valid()) {
		printf("  #%d %p %08lx\n", count++, dump.ip(), *dump.ip());
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


static void init_core_page_fault_handler()
{
	/* create echo EC */
	enum {
		CPU_NO     = 0,
		GLOBAL     = false,
		EXC_BASE   = 0
	};

	addr_t ec_sel = cap_selector_allocator()->alloc();

	uint8_t ret = create_ec(ec_sel, __core_pd_sel, CPU_NO,
	                        CORE_PAGER_UTCB_ADDR, core_pager_stack_top(),
	                        EXC_BASE, GLOBAL);
	if (ret)
		PDBG("create_ec returned %u", ret);

	/* set up page-fault portal */
	create_pt(PT_SEL_PAGE_FAULT, __core_pd_sel, ec_sel,
	          Mtd(Mtd::QUAL | Mtd::ESP | Mtd::EIP),
	          (addr_t)page_fault_handler);

	/* startup portal for global core threads */
	create_pt(PT_SEL_STARTUP, __core_pd_sel, ec_sel,
	          Mtd(Mtd::EIP | Mtd::ESP),
	          (addr_t)startup_handler);
}


/**************
 ** Platform **
 **************/

Platform::Platform() :
	_io_mem_alloc(core_mem_alloc()), _io_port_alloc(core_mem_alloc()),
	_irq_alloc(core_mem_alloc()),
	_vm_base(0x1000), _vm_size(0)
{
	Hip  *hip  = (Hip *)__initial_sp;

	/* register UTCB of main thread */
	__main_thread_utcb = (Utcb *)(__initial_sp - get_page_size());

	/* set core pd selector */
	__core_pd_sel = hip->sel_exc;

	/* create lock used by capability allocator */
	Nova::create_sm(Nova::SM_SEL_EC, __core_pd_sel, 0);

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

	/* sanity checks */
	if (hip->sel_exc + 3 > NUM_INITIAL_PT_RESERVED) {
		printf("configuration error\n");
		nova_die();
	}

	/* configure virtual address spaces */
#ifdef __x86_64__
	_vm_size = 0x800000000000UL - _vm_base;
#else
	_vm_size = 0xc0000000UL - _vm_base;
#endif

	/* set up page fault handler for core - for debugging */
	init_core_page_fault_handler();

	if (verbose_boot_info) {
		printf("Hypervisor %s VMX\n", hip->has_feature_vmx() ? "features" : "does not feature");
		printf("Hypervisor %s SVM\n", hip->has_feature_svm() ? "features" : "does not feature");
	}

	/* initialize core allocators */
	size_t num_mem_desc = (hip->hip_length - hip->mem_desc_offset)
	                      / hip->mem_desc_size;

	if (verbose_boot_info)
		printf("Hypervisor info page contains %zd memory descriptors:\n", num_mem_desc);

	addr_t mem_desc_base = ((addr_t)hip + hip->mem_desc_offset);

	/* define core's virtual address space */
	addr_t virt_beg = _vm_base;
	addr_t virt_end = _vm_size;
	_core_mem_alloc.virt_alloc()->add_range(virt_beg, virt_end - virt_beg);

	/* exclude core image from core's virtual address allocator */
	addr_t core_virt_beg = trunc_page((addr_t)&_prog_img_beg);
	addr_t core_virt_end = round_page((addr_t)&_prog_img_end);
	size_t core_size     = core_virt_end - core_virt_beg;
	region_alloc()->remove_range(core_virt_beg, core_size);

	/* preserve Bios Data Area (BDA) in core's virtual address space */
	region_alloc()->remove_range(BDA_VIRT_ADDR, 0x1000);

	/* preserve context area in core's virtual address space */
	region_alloc()->remove_range(Native_config::context_area_virtual_base(),
	                             Native_config::context_area_virtual_size());

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
		if (Native_config::context_area_virtual_base() <= check[i] &&
			check[i] < Native_config::context_area_virtual_base() +
			Native_config::context_area_virtual_size())
		{
			PERR("overlapping area - [%lx, %lx) vs %lx",
			     Native_config::context_area_virtual_base(),
			     Native_config::context_area_virtual_base() +
			     Native_config::context_area_virtual_size(), check[i]);
			nova_die();
 		}
	}
 
	/* initialize core's physical-memory and I/O memory allocator */
	_io_mem_alloc.add_range(0, ~0xfffUL);
	Hip::Mem_desc *mem_desc = (Hip::Mem_desc *)mem_desc_base;
	for (unsigned i = 0; i < num_mem_desc; i++, mem_desc++) {
		if (mem_desc->type != Hip::Mem_desc::AVAILABLE_MEMORY) continue;

		if (verbose_boot_info)
			printf("detected physical memory: 0x%16llx - size: 0x%llx\n",
			        mem_desc->addr, mem_desc->size);

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
			printf("use      physical memory: 0x%16lx - size: 0x%zx\n", base, size);

		_io_mem_alloc.remove_range(base, size);
		_core_mem_alloc.phys_alloc()->add_range(base, size);
	}

	/* exclude all non-available memory from physical allocator */
	mem_desc = (Hip::Mem_desc *)mem_desc_base;
	for (unsigned i = 0; i < num_mem_desc; i++, mem_desc++) {
		if (mem_desc->type == Hip::Mem_desc::AVAILABLE_MEMORY) continue;

		/* skip regions above 4G on 32 bit, no op on 64 bit */
		if (mem_desc->addr > ~0UL) continue;

		addr_t base = trunc_page(mem_desc->addr);
		size_t size;
		/* truncate size if base+size larger then natural 32/64 bit boundary */
		if (mem_desc->addr >= ~0UL - mem_desc->size + 1)
			size = round_page(~0UL - mem_desc->addr + 1);
		else
			size = round_page(mem_desc->addr + mem_desc->size) - base;

		_io_mem_alloc.add_range(base, size);
		_core_mem_alloc.phys_alloc()->remove_range(base, size);
	}

	/* needed as I/O memory by the VESA driver */
	_io_mem_alloc.add_range(0, 0x1000);
	_core_mem_alloc.phys_alloc()->remove_range(0, 0x1000);

	/* exclude pages holding multi-boot command lines from core allocators */
	mem_desc = (Hip::Mem_desc *)mem_desc_base;
	addr_t prev_cmd_line_page = 0, curr_cmd_line_page = 0;
	for (unsigned i = 0; i < num_mem_desc; i++, mem_desc++) {
		if (mem_desc->type != Hip::Mem_desc::MULTIBOOT_MODULE) continue;
		if (!mem_desc->aux) continue;

		curr_cmd_line_page = mem_desc->aux >> get_page_size_log2();
		if (curr_cmd_line_page == prev_cmd_line_page) continue;

		ram_alloc()->remove_range(curr_cmd_line_page << get_page_size_log2(),
		                          get_page_size());
		prev_cmd_line_page = curr_cmd_line_page;
	}

	/* preserve page following the last multi-boot command line */
	ram_alloc()->remove_range((curr_cmd_line_page + 1) << get_page_size_log2(),
	                           get_page_size());

	/*
	 * From now on, it is save to use the core allocators...
	 */

	/* build ROM file system */
	mem_desc = (Hip::Mem_desc *)mem_desc_base;
	prev_cmd_line_page = 0, curr_cmd_line_page = 0;
	addr_t mapped_cmd_line = 0;
	for (unsigned i = 0; i < num_mem_desc; i++, mem_desc++) {
		if (mem_desc->type != Hip::Mem_desc::MULTIBOOT_MODULE) continue;
		if (!mem_desc->addr || !mem_desc->size || !mem_desc->aux) continue;

		addr_t core_local_addr =
			_map_page(trunc_page(mem_desc->addr) >> get_page_size_log2(),
		              (round_page(mem_desc->addr + mem_desc->size) -
			          trunc_page(mem_desc->addr)) >> get_page_size_log2());
		if (!core_local_addr) {
			PERR("could not map multi boot module");
			nova_die();
		}
		/* adjust module addr if it is not page aligned */
		core_local_addr += mem_desc->addr - trunc_page(mem_desc->addr);

		printf("map multi-boot module: physical 0x%8lx -> [0x%8lx-0x%8lx) - ",
		       (addr_t)mem_desc->addr, (addr_t)core_local_addr,
		       (addr_t)(core_local_addr + mem_desc->size));

		/* check if cmd line is part of the module pages, don't map it twice */
		addr_t aux;
		if (trunc_page(mem_desc->addr) <= mem_desc->aux &&
		    mem_desc->aux < round_page(mem_desc->addr + mem_desc->size)) {
			aux = core_local_addr + (mem_desc->aux - mem_desc->addr);
		} else {	
			curr_cmd_line_page     = mem_desc->aux >> get_page_size_log2();
			if (curr_cmd_line_page != prev_cmd_line_page) {
				mapped_cmd_line = _map_page(curr_cmd_line_page, 2);
				prev_cmd_line_page = curr_cmd_line_page;
			}
			aux = mapped_cmd_line + (mem_desc->aux - trunc_page(mem_desc->aux));
		}
		const char *name = commandline_to_basename(reinterpret_cast<char *>(aux));

		printf("%s\n", name);

		Rom_module *rom_module = new (core_mem_alloc())
		                         Rom_module(core_local_addr, mem_desc->size, name);
		_rom_fs.insert(rom_module);

	}

	/* export hypervisor info page as ROM module */
	_rom_fs.insert(new (core_mem_alloc())
	               Rom_module((addr_t)hip, get_page_size(), "hypervisor_info_page"));

	/* I/O port allocator (only meaningful for x86) */
	_io_port_alloc.add_range(0, 0x10000);

	/* IRQ allocator */
	_irq_alloc.add_range(0, hip->sel_gsi - 1);
	_gsi_base_sel = (hip->mem_desc_offset - hip->cpu_desc_offset) / hip->cpu_desc_size;

	/* remap main utcb to default utbc address */
	if (map_local(__main_thread_utcb, (addr_t)__main_thread_utcb,
	              (addr_t)main_thread_utcb(), 1, Rights(true, true, false))) {
		PERR("could not remap main threads utcb");
		nova_die();
	}

	if (verbose_boot_info) {
		printf(":virt_alloc: "); _core_mem_alloc.virt_alloc()->raw()->dump_addr_tree();
		printf(":phys_alloc: "); _core_mem_alloc.phys_alloc()->raw()->dump_addr_tree();
		printf(":io_mem_alloc: "); _io_mem_alloc.raw()->dump_addr_tree();
	}
}


/****************************************
 ** Support for core memory management **
 ****************************************/

bool Core_mem_allocator::Mapped_mem_allocator::_map_local(addr_t virt_addr,
                                                          addr_t phys_addr,
                                                          unsigned size_log2)
{
	map_local((Utcb *)Thread_base::myself()->utcb(), phys_addr,
	          virt_addr, 1 << (size_log2 - get_page_size_log2()),
	          Rights(true, true, true), true);
	return true;
}


/********************************
 ** Generic platform interface **
 ********************************/

void Platform::wait_for_exit() { sleep_forever(); }


void Core_parent::exit(int exit_value) { }
