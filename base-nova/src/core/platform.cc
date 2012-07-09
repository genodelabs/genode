/*
 * \brief  Platform interface implementation
 * \author Norman Feske
 * \author Sebastian Sumpf
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


/**
 * Initial value of esp register, saved by the crt0 startup code
 *
 * This value contains the address of the hypervisor information page.
 */
extern long __initial_sp;


/**
 * First available capability selector for custom use
 */
extern int __first_free_cap_selector;


/**
 * Pointer to the UTCB of the main thread
 */
extern Utcb *__main_thread_utcb;


/**
 * Virtual address range consumed by core's program image
 */
extern unsigned _prog_img_beg, _prog_img_end;


/**
 *  Capability selector of root PD
 */
extern int __local_pd_sel;

/**
 * Preserve physical page for the exclusive (read-only) use by core
 */
void Platform::_preserve_page(addr_t phys_page)
{
	/* locally map page one-to-one */
	map_local_one_to_one(__main_thread_utcb,
	                     Mem_crd(phys_page, 0,
	                             Rights(true, true, false)));

	/* remove page with command line from physical-memory allocator */
	addr_t addr = phys_page*get_page_size();
	_core_mem_alloc.phys_alloc()->remove_range(addr, get_page_size());
	_core_mem_alloc.virt_alloc()->remove_range(addr, get_page_size());
}


/*****************************
 ** Core page-fault handler **
 *****************************/

enum { CORE_PAGER_UTCB_ADDR = 0x50002000 };


/**
 * IDC handler for the page-fault portal
 */
static void page_fault_handler()
{
	Utcb *utcb = (Utcb *)CORE_PAGER_UTCB_ADDR;

	addr_t pf_addr = utcb->qual[1];
	addr_t pf_eip  = utcb->eip;
	addr_t pf_esp  = utcb->esp;

	printf("\nPAGE-FAULT IN CORE: ADDR %lx  IP %lx  SP %lx stack trace follows...\n",
	       pf_addr, pf_eip, pf_esp);

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
	printf("  #%d %08lx %08lx\n", count++, pf_esp, pf_eip);

	Core_img dump(pf_esp);
	while (dump.ip_valid()) {
		printf("  #%d %p %08lx\n", count++, dump.ip(), *dump.ip());
		dump.next_ip();
	}

	sleep_forever();
}


static void init_core_page_fault_handler()
{
	/* create echo EC */
	enum { 
		STACK_SIZE = 4*1024,
		CPU_NO     = 0,
		GLOBAL     = false,
		EXC_BASE   = 0
	};

	static char stack[STACK_SIZE];

	mword_t sp = (long)&stack[STACK_SIZE - sizeof(long)];
	int ec_sel = cap_selector_allocator()->alloc();

	int ret = create_ec(ec_sel, __local_pd_sel, CPU_NO, CORE_PAGER_UTCB_ADDR,
	                    (mword_t)sp, EXC_BASE, GLOBAL);
	if (ret)
		PDBG("create_ec returned %d", ret);

	/* set up page-fault portal */
	create_pt(PT_SEL_PAGE_FAULT, __local_pd_sel, ec_sel,
	          Mtd(Mtd::QUAL | Mtd::ESP | Mtd::EIP),
	          (mword_t)page_fault_handler);
}


/**************
 ** Platform **
 **************/

Platform::Platform() :
	_io_mem_alloc(core_mem_alloc()), _io_port_alloc(core_mem_alloc()),
	_irq_alloc(core_mem_alloc()),
	_vm_base(0), _vm_size(0)
{
	Hip  *hip  = (Hip *)__initial_sp;

	/* register UTCB of main thread */
	__main_thread_utcb = (Utcb *)(__initial_sp - get_page_size());

	/* register start of usable capability range */
	__first_free_cap_selector = hip->sel_exc + hip->sel_gsi + 3;

	/* set core pd selector */
	__local_pd_sel = hip->sel_exc;

	/* locally map the whole I/O port range */
	enum { ORDER_64K = 16 };
	map_local_one_to_one(__main_thread_utcb, Io_crd(0, ORDER_64K));

	/*
	 * Now that we can access the I/O ports for comport 0, printf works...
	 */

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
	addr_t virt_beg = get_page_size();
	addr_t virt_end = Thread_base::CONTEXT_AREA_VIRTUAL_BASE;
	_core_mem_alloc.virt_alloc()->add_range(virt_beg,
	                                        virt_end - virt_beg);

	/* exclude core image from core's virtual address allocator */
	addr_t core_virt_beg = trunc_page((addr_t)&_prog_img_beg),
	                       core_virt_end = round_page((addr_t)&_prog_img_end);
	size_t core_size     = core_virt_end - core_virt_beg;
	_core_mem_alloc.virt_alloc()->remove_range(core_virt_beg, core_size);

	/* preserve context area in core's virtual address space */
	_core_mem_alloc.virt_alloc()->remove_range(Thread_base::CONTEXT_AREA_VIRTUAL_BASE,
	                                           Thread_base::CONTEXT_AREA_VIRTUAL_SIZE);

	/* initialize core's physical-memory and I/O memory allocator */
	_io_mem_alloc.add_range(0, ~0xfff);
	Hip::Mem_desc *mem_desc = (Hip::Mem_desc *)mem_desc_base;
	for (unsigned i = 0; i < num_mem_desc; i++, mem_desc++) {
		if (mem_desc->type != Hip::Mem_desc::AVAILABLE_MEMORY) continue;

		if (verbose_boot_info)
			printf("detected physical memory: 0x%llx - 0x%llx\n", mem_desc->addr, mem_desc->size);

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
			printf("use      physical memory: 0x%lx - 0x%zx\n", base, size);

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

		_preserve_page(curr_cmd_line_page);
		prev_cmd_line_page = curr_cmd_line_page;
	}

	/* preserve page following the last multi-boot command line */
	_preserve_page(curr_cmd_line_page + 1);

	/*
	 * From now on, it is save to use the core allocators...
	 */

	/* build ROM file system */
	mem_desc = (Hip::Mem_desc *)mem_desc_base;
	for (unsigned i = 0; i < num_mem_desc; i++, mem_desc++) {
		if (mem_desc->type != Hip::Mem_desc::MULTIBOOT_MODULE) continue;
		if (!mem_desc->addr || !mem_desc->size || !mem_desc->aux) continue;

		addr_t aux = mem_desc->aux;
		const char *name = commandline_to_basename(reinterpret_cast<char *>(aux));
		printf("detected multi-boot module: %s 0x%lx-0x%lx\n", name,
		       (long)mem_desc->addr, (long)(mem_desc->addr + mem_desc->size - 1));

		void *core_local_addr = (void*)0x234;
		if (!region_alloc()->alloc(round_page(mem_desc->size), &core_local_addr))
			PERR("could not locally map multi-boot module");

		int res = map_local(__main_thread_utcb, mem_desc->addr, (addr_t)core_local_addr,
		                    round_page(mem_desc->size) >> get_page_size_log2(), true);
		if (res)
			PERR("map_local failed res=%d", res);

		Rom_module *rom_module = new (core_mem_alloc())
		                         Rom_module((addr_t)core_local_addr, mem_desc->size, name);
		_rom_fs.insert(rom_module);

		/* zero remainder of last ROM page */
		size_t count = 0x1000 - rom_module->size() % 0x1000;
		if (count != 0x1000)
			memset(reinterpret_cast<void *>(rom_module->addr() + rom_module->size()), 0, count);

	}

	/* export hypervisor info page as ROM module */
	_rom_fs.insert(new (core_mem_alloc())
	               Rom_module((addr_t)hip, get_page_size(), "hypervisor_info_page"));

	/* configure non-core virtual address spaces as 2G-2G split */
	_vm_base = get_page_size();
	_vm_size = 2*1024*1024*1024UL - _vm_base;

	/* I/O port allocator (only meaningful for x86) */
	_io_port_alloc.add_range(0, 0x10000);

	/* IRQ allocator */
	_irq_alloc.add_range(0, hip->sel_gsi - 1);
	_gsi_base_sel = (hip->mem_desc_offset - hip->cpu_desc_offset) / hip->cpu_desc_size;

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
	          virt_addr, 1 << (size_log2 - get_page_size_log2()), true);
	return true;
}


/********************************
 ** Generic platform interface **
 ********************************/

void Platform::wait_for_exit() { sleep_forever(); }


void Core_parent::exit(int exit_value) { }
