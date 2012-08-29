/*
 * \brief   Platform interface implementation
 * \author  Norman Feske
 * \date    2009-10-02
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
#include <map_local.h>

/* Codezero includes */
#include <codezero/syscalls.h>

using namespace Genode;

enum { verbose_boot_info = true };

/*
 * Memory-layout information provided by the linker script
 */

/* virtual address range consumed by core's program image */
extern unsigned _prog_img_beg, _prog_img_end;

/* physical address range occupied by core */
extern addr_t _vma_start, _lma_start;


/**************************
 ** Boot-module handling **
 **************************/

/**
 * Scan ROM module image for boot modules
 *
 * By convention, the boot modules start at the page after core's BSS segment.
 */
int Platform::_init_rom_fs()
{
	/**
	 * Format of module meta-data as found in the ROM module image
	 */
	struct Module
	{
		long name;  /* physical address of null-terminated string */
		long base;  /* physical address of module data */
		long size;  /* size of module data in bytes */
	};

	/* find base address of ROM module image */
	addr_t phys_base = round_page((addr_t)&_prog_img_end);

	/* map the first page of the image containing the module meta data */
	class Out_of_virtual_memory_during_rom_fs_init { };
	void *virt_base = 0;
	if (!_core_mem_alloc.virt_alloc()->alloc(get_page_size(), &virt_base))
		throw Out_of_virtual_memory_during_rom_fs_init();

	if (!map_local(phys_base, (addr_t)virt_base, 1)) {
		PERR("map_local failed");
		return -1;
	}

	/* remove page containing module infos from physical memory allocator */
	_core_mem_alloc.phys_alloc()->remove_range(phys_base, get_page_size());

	/* validate the presence of a ROM image by checking the magic cookie */
	const char cookie[4] = {'G', 'R', 'O', 'M'};
	for (size_t i = 0; i < sizeof(cookie); i++)
		if (cookie[i] != ((char *)virt_base)[i]) {
			PERR("could not detect ROM modules");
			return -2;
		}

	printf("detected ROM module image at 0x%p\n", (void *)phys_base);

	/* detect overly large meta data, we only support 4K */
	addr_t end_of_header = ((long *)virt_base)[1];
	size_t header_size = end_of_header - (long)phys_base;
	if (header_size > get_page_size()) {
		PERR("ROM fs module header exceeds %d bytes", get_page_size());
		return -3;
	}

	/* start of module list */
	Module *module = (Module *)((addr_t)virt_base + 2*sizeof(long));

	/*
	 * Interate over module list and populate core's ROM file system with
	 * 'Rom_module' objects.
	 */
	for (; module->name; module++) {

		/* convert physical address of module name to core-local address */
		char *name = (char *)(module->name - phys_base + (addr_t)virt_base);

		printf("ROM module \"%s\" at physical address 0x%p, size=%zd\n",
		       name, (void *)module->base, (size_t)module->size);

		Rom_module *rom_module = new (core_mem_alloc())
			Rom_module(module->base, module->size, name);

		_rom_fs.insert(rom_module);

		/* remove module from physical memory allocator */
		_core_mem_alloc.phys_alloc()->remove_range(module->base, round_page(module->size));
	}
	return 0;
}


/****************************************
 ** Support for core memory management **
 ****************************************/

bool Core_mem_allocator::Mapped_mem_allocator::_map_local(addr_t virt_addr, addr_t phys_addr, unsigned size_log2)
{
	return map_local(phys_addr, virt_addr, 1 << (size_log2 - get_page_size_log2()));
}


/************************
 ** Platform interface **
 ************************/

Platform::Platform() :
	_io_mem_alloc(core_mem_alloc()), _io_port_alloc(core_mem_alloc()),
	_irq_alloc(core_mem_alloc()), _vm_base(0), _vm_size(0)
{
	using namespace Codezero;

	/* init core UTCB */
	static char main_utcb[PAGE_SIZE] __attribute__((aligned(PAGE_SIZE)));
	static struct exregs_data exregs;
	exregs_set_utcb(&exregs, (unsigned long)&main_utcb[0]);
	l4_exchange_registers(&exregs, thread_myself());

	/* error handling is futile at this point */

	/* read number of capabilities */
	int num_caps;
	int ret;
	if ((ret = l4_capability_control(CAP_CONTROL_NCAPS,
	                                 0, &num_caps)) < 0) {
		PERR("l4_capability_control(CAP_CONTROL_NCAPS) returned %d", ret);
		class Could_not_obtain_num_of_capabilities { };
		throw Could_not_obtain_num_of_capabilities();
	}

	struct capability cap_array[num_caps];

	if (verbose_boot_info)
		printf("allocated cap array[%d] of size %d on stack\n",
		       num_caps, sizeof(cap_array));

	/* read all capabilities */
	if ((ret = l4_capability_control(CAP_CONTROL_READ,
	                                 0, cap_array)) < 0) {
		PERR("l4_capability_control(CAP_CONTROL_READ) returned %d", ret);
		class Read_caps_failed { };
		throw Read_caps_failed();
	}

	/* initialize core allocators */
	bool phys_mem_defined = false;
	addr_t dev_mem_base = 0;
	for (int i = 0; i < num_caps; i++) {
		struct capability *cap = &cap_array[i];

		addr_t base = cap->start << get_page_size_log2(),
		       size = cap->size  << get_page_size_log2();

		if (verbose_boot_info)
			printf("cap type=%x, rtype=%x, base=%lx, size=%lx\n",
			       cap_type(cap), cap_rtype(cap), base, size);

		switch (cap_type(cap)) {

		case CAP_TYPE_MAP_VIRTMEM:

			/*
			 * Use first non-UTCB virtual address range as default
			 * virtual memory range usable for all processes.
			 */
			if (_vm_size == 0) {

				/* exclude page at virtual address 0 */
				if (base == 0 && size >= get_page_size()) {
					base += get_page_size();
					size -= get_page_size();
				}

				_vm_base = base;
				_vm_size = size;

				/* add range as free range to core's virtual address allocator */
				_core_mem_alloc.virt_alloc()->add_range(base, size);
				break;
			}

			PWRN("ignoring additional virtual address range [%lx,%lx)",
			     base, base + size);
			break;

		case CAP_TYPE_MAP_PHYSMEM:

			/*
			 * We interpret the first physical memory resource that is bigger
			 * than typical device resources as RAM.
			 */
			enum { RAM_SIZE_MIN = 16*1024*1024 };
			if (!phys_mem_defined && size > RAM_SIZE_MIN) {
				_core_mem_alloc.phys_alloc()->add_range(base, size);
				phys_mem_defined = true;
				dev_mem_base = base + size;
			}
			break;

		case CAP_TYPE_IPC:
		case CAP_TYPE_UMUTEX:
		case CAP_TYPE_IRQCTRL:
		case CAP_TYPE_QUANTITY:
			break;
		}
	}

	addr_t core_virt_beg = trunc_page((addr_t)&_prog_img_beg),
	       core_virt_end = round_page((addr_t)&_prog_img_end);
	size_t core_size     = core_virt_end - core_virt_beg;

	printf("core image:\n");
	printf("  virtual address range [%08lx,%08lx) size=0x%zx\n",
	       core_virt_beg, core_virt_end, core_size);
	printf("  physically located at 0x%08lx\n", _lma_start);

	/* remove core image from core's virtual address allocator */
	_core_mem_alloc.virt_alloc()->remove_range(core_virt_beg, core_size);

	/* preserve context area in core's virtual address space */
	_core_mem_alloc.virt_alloc()->raw()->remove_range(Native_config::context_area_virtual_base(),
	                                                  Native_config::context_area_virtual_size());

	/* remove used core memory from physical memory allocator */
	_core_mem_alloc.phys_alloc()->remove_range(_lma_start, core_size);

	/* remove magically mapped UART from core virtual memory */
	_core_mem_alloc.virt_alloc()->remove_range(USERSPACE_CONSOLE_VBASE, get_page_size());

	/* add boot modules to ROM fs */
	if (_init_rom_fs() < 0) {
		PERR("initialization of romfs failed - halt.");
		while(1);
	}

	/* initialize interrupt allocator */
	_irq_alloc.add_range(0, 255);

	/* regard physical addresses higher than memory area as MMIO */
	_io_mem_alloc.add_range(dev_mem_base, 0x80000000 - dev_mem_base);

	/*
	 * Print statistics about allocator initialization
	 */
	printf("VM area at [%08lx,%08lx)\n", _vm_base, _vm_base + _vm_size);

	if (verbose_boot_info) {
		printf(":phys_alloc:   "); _core_mem_alloc.phys_alloc()->raw()->dump_addr_tree();
		printf(":virt_alloc:   "); _core_mem_alloc.virt_alloc()->raw()->dump_addr_tree();
		printf(":io_mem_alloc: "); _io_mem_alloc.raw()->dump_addr_tree();
	}
}


void Platform::wait_for_exit()
{
	sleep_forever();
}


void Core_parent::exit(int exit_value) { }
