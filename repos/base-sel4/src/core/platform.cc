/*
 * \brief   Platform interface implementation
 * \author  Norman Feske
 * \date    2015-05-01
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
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
#include <cnode.h>

using namespace Genode;

static bool const verbose_boot_info = true;


/*
 * Memory-layout information provided by the linker script
 */

/* virtual address range consumed by core's program image */
extern unsigned _prog_img_beg, _prog_img_end;


/****************************************
 ** Support for core memory management **
 ****************************************/

bool Core_mem_allocator::Mapped_mem_allocator::_map_local(addr_t virt_addr,
                                                          addr_t phys_addr,
                                                          unsigned size)
{
	return map_local(phys_addr, virt_addr, size / get_page_size());
}


bool Core_mem_allocator::Mapped_mem_allocator::_unmap_local(addr_t virt_addr,
                                                            unsigned size)
{
	return unmap_local(virt_addr, size / get_page_size());
}


/************************
 ** Platform interface **
 ************************/

/**
 * Initialize allocator with untyped memory ranges according to the boot info
 */
static void init_allocator(Range_allocator &alloc, seL4_BootInfo const &bi,
                           unsigned const start_idx, unsigned const num_idx)
{
	for (unsigned i = start_idx; i < start_idx + num_idx; i++) {

		/* index into 'untypedPaddrList' and 'untypedSizeBitsList' */
		unsigned const k = i - bi.untyped.start;

		addr_t const base = bi.untypedPaddrList[k];
		size_t const size = 1UL << bi.untypedSizeBitsList[k];

		alloc.add_range(base, size);
	}
}


static inline void init_sel4_ipc_buffer()
{
	asm volatile ("movl %0, %%gs" :: "r"(IPCBUF_GDT_SELECTOR) : "memory");
}


/* CNode dimensions */
enum {
	NUM_TOP_SEL_LOG2  = 12UL,
	NUM_CORE_SEL_LOG2 = 14UL,
	NUM_PHYS_SEL_LOG2 = 20UL,
};


/* selectors for statically created CNodes */
enum Static_cnode_sel {
	TOP_CNODE_SEL      = 0x200,
	CORE_PAD_CNODE_SEL = 0x201,
	CORE_CNODE_SEL     = 0x202,
	PHYS_CNODE_SEL     = 0x203
};


/* indices within top-level CNode */
enum Top_cnode_idx {
	TOP_CNODE_CORE_IDX = 0,
	TOP_CNODE_PHYS_IDX = 0xfff
};


/**
 * Replace initial CSpace with custom CSpace layout
 */
static void switch_to_core_cspace(Range_allocator &phys_alloc)
{
	Cnode_base const initial_cspace(seL4_CapInitThreadCNode, 32);

	/* allocate 1st-level CNode */
	static Cnode top_cnode(TOP_CNODE_SEL, NUM_TOP_SEL_LOG2, phys_alloc);

	/* allocate 2nd-level CNode to align core's CNode with the LSB of the CSpace*/
	static Cnode core_pad_cnode(CORE_PAD_CNODE_SEL,
	                            32UL - NUM_TOP_SEL_LOG2 - NUM_CORE_SEL_LOG2,
	                            phys_alloc);

	/* allocate 3rd-level CNode for core's objects */
	static Cnode core_cnode(CORE_CNODE_SEL, NUM_CORE_SEL_LOG2, phys_alloc);

	/* copy initial selectors to core's CNode */
	core_cnode.copy(initial_cspace, seL4_CapInitThreadTCB);
	core_cnode.copy(initial_cspace, seL4_CapInitThreadCNode);
	core_cnode.copy(initial_cspace, seL4_CapInitThreadPD);
	core_cnode.move(initial_cspace, seL4_CapIRQControl); /* cannot be copied */
	core_cnode.copy(initial_cspace, seL4_CapIOPort);
	core_cnode.copy(initial_cspace, seL4_CapBootInfoFrame);
	core_cnode.copy(initial_cspace, seL4_CapArchBootInfoFrame);
	core_cnode.copy(initial_cspace, seL4_CapInitThreadIPCBuffer);
	core_cnode.copy(initial_cspace, seL4_CapIPI);
	core_cnode.copy(initial_cspace, seL4_CapDomain);

	/* copy untyped memory selectors to core's CNode */
	seL4_BootInfo const &bi = sel4_boot_info();

	for (unsigned sel = bi.untyped.start; sel < bi.untyped.end; sel++)
		core_cnode.copy(initial_cspace, sel);

	for (unsigned sel = bi.deviceUntyped.start; sel < bi.deviceUntyped.end; sel++)
		core_cnode.copy(initial_cspace, sel);

	/* copy statically created CNode selectors to core's CNode */
	core_cnode.copy(initial_cspace, TOP_CNODE_SEL);
	core_cnode.copy(initial_cspace, CORE_PAD_CNODE_SEL);
	core_cnode.copy(initial_cspace, CORE_CNODE_SEL);

	/*
	 * Construct CNode hierarchy of core's CSpace
	 */

	/* insert 3rd-level core CNode into 2nd-level core-pad CNode */
	core_pad_cnode.copy(initial_cspace, CORE_CNODE_SEL, 0);

	/* insert 2nd-level core-pad CNode into 1st-level CNode */
	top_cnode.copy(initial_cspace, CORE_PAD_CNODE_SEL, TOP_CNODE_CORE_IDX);

	/* allocate 2nd-level CNode for storing page-frame cap selectors */
	static Cnode phys_cnode(PHYS_CNODE_SEL, NUM_PHYS_SEL_LOG2, phys_alloc);

	/* insert 2nd-level phys-mem CNode into 1st-level CNode */
	top_cnode.copy(initial_cspace, PHYS_CNODE_SEL, TOP_CNODE_PHYS_IDX);

	/* activate core's CSpace */
	{
		seL4_CapData_t null_data = { { 0 } };

		int const ret = seL4_TCB_SetSpace(seL4_CapInitThreadTCB,
		                                  seL4_CapNull, /* fault_ep */
		                                  TOP_CNODE_SEL, null_data,
		                                  seL4_CapInitThreadPD, null_data);

		if (ret != 0) {
			PERR("%s: seL4_TCB_SetSpace returned %d", __FUNCTION__, ret);
		}
	}
}


Platform::Platform()
:
	_io_mem_alloc(core_mem_alloc()), _io_port_alloc(core_mem_alloc()),
	_irq_alloc(core_mem_alloc()),
	_vm_base(0x1000),
	_vm_size(2*1024*1024*1024UL - _vm_base) /* use the lower 2GiB */
{
	/*
	 * Initialize core allocators
	 */
	seL4_BootInfo const &bi = sel4_boot_info();

	/* interrupt allocator */
	_irq_alloc.add_range(0, 255);

	/* physical memory ranges */
	init_allocator(*_core_mem_alloc.phys_alloc(), bi, bi.untyped.start,
	               bi.untyped.end - bi.untyped.start);

	/* I/O memory ranges */
	init_allocator(_io_mem_alloc, bi, bi.deviceUntyped.start,
	               bi.deviceUntyped.end - bi.deviceUntyped.start);

	/* core's virtual memory */
	_core_mem_alloc.virt_alloc()->add_range(_vm_base, _vm_size);

	/* remove core image from core's virtual address allocator */
	addr_t const core_virt_beg = trunc_page((addr_t)&_prog_img_beg),
	             core_virt_end = round_page((addr_t)&_prog_img_end);
	size_t const core_size     = core_virt_end - core_virt_beg;

	_core_mem_alloc.virt_alloc()->remove_range(core_virt_beg, core_size);

	if (verbose_boot_info) {
		printf("core image:\n");
		printf("  virtual address range [%08lx,%08lx) size=0x%zx\n",
		       core_virt_beg, core_virt_end, core_size);
	}

	/* preserve context area in core's virtual address space */
	_core_mem_alloc.virt_alloc()->remove_range(Native_config::context_area_virtual_base(),
	                                           Native_config::context_area_virtual_size());

	/*
	 * Until this point, no interaction with the seL4 kernel was needed.
	 * However, the next steps involve the invokation of system calls and
	 * the use of kernel services. To use the kernel bindings, we first
	 * need to initialize the TLS mechanism that is used to find the IPC
	 * buffer for the calling thread.
	 */
	init_sel4_ipc_buffer();

	/* initialize core's capability space */
	switch_to_core_cspace(*_core_mem_alloc.phys_alloc());

	/* add boot modules to ROM fs */

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
