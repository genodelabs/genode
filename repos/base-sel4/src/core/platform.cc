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


/******************
 ** Boot modules **
 ******************/

struct Boot_module_header
{
	char   const *name;  /* physical address of null-terminated string */
	addr_t const  base;  /* physical address of module data */
	size_t const  size;  /* size of module data in bytes */
};

extern Boot_module_header _boot_modules_headers_begin;
extern Boot_module_header _boot_modules_headers_end;
extern int                _boot_modules_binaries_begin;
extern int                _boot_modules_binaries_end;


/****************************************
 ** Support for core memory management **
 ****************************************/

bool Core_mem_allocator::Mapped_mem_allocator::_map_local(addr_t virt_addr,
                                                          addr_t phys_addr,
                                                          unsigned size)
{
	size_t const num_pages = size / get_page_size();

	Untyped_memory::convert_to_page_frames(phys_addr, num_pages);

	return map_local(phys_addr, virt_addr, num_pages);
}


bool Core_mem_allocator::Mapped_mem_allocator::_unmap_local(addr_t virt_addr,
                                                            unsigned size)
{
	return unmap_local(virt_addr, size / get_page_size());
}


/************************
 ** Platform interface **
 ************************/

void Platform::_init_unused_phys_alloc()
{
	_unused_phys_alloc.add_range(0, ~0UL);
}


/**
 * Initialize allocator with untyped memory ranges according to the boot info
 */
static void init_allocator(Range_allocator &alloc,
                           Range_allocator &unused_phys_alloc,
                           seL4_BootInfo const &bi,
                           unsigned const start_idx, unsigned const num_idx)
{
	for (unsigned i = start_idx; i < start_idx + num_idx; i++) {

		/* index into 'untypedPaddrList' and 'untypedSizeBitsList' */
		unsigned const k = i - bi.untyped.start;

		addr_t const base = bi.untypedPaddrList[k];
		size_t const size = 1UL << bi.untypedSizeBitsList[k];

		alloc.add_range(base, size);

		unused_phys_alloc.remove_range(base, size);
	}
}


static inline void init_sel4_ipc_buffer()
{
	asm volatile ("movl %0, %%gs" :: "r"(IPCBUF_GDT_SELECTOR) : "memory");
}


void Platform::_init_allocators()
{
	seL4_BootInfo const &bi = sel4_boot_info();

	/* interrupt allocator */
	_irq_alloc.add_range(0, 255);

	/* physical memory ranges */
	init_allocator(*_core_mem_alloc.phys_alloc(), _unused_phys_alloc,
	               bi, bi.untyped.start,
	               bi.untyped.end - bi.untyped.start);

	/* I/O memory ranges */
	init_allocator(_io_mem_alloc, _unused_phys_alloc,
	               bi, bi.deviceUntyped.start,
	               bi.deviceUntyped.end - bi.deviceUntyped.start);

	/* core's virtual memory */
	_core_mem_alloc.virt_alloc()->add_range(_vm_base, _vm_size);

	/* remove core image from core's virtual address allocator */

	/*
	 * XXX Why do we need to skip a few KiB after the end of core?
	 *     When allocating a PTE immediately after _prog_img_end, the
	 *     kernel would complain "Mapping already present" on the
	 *     attempt to map a page frame.
	 */
	addr_t const core_virt_beg = trunc_page((addr_t)&_prog_img_beg),
	             core_virt_end = round_page((addr_t)&_boot_modules_binaries_end)
	                           + 64*1024;
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
}


void Platform::_switch_to_core_cspace()
{
	Cnode_base const initial_cspace(seL4_CapInitThreadCNode, 32);

	/* copy initial selectors to core's CNode */
	_core_cnode.copy(initial_cspace, seL4_CapInitThreadTCB);
	_core_cnode.copy(initial_cspace, seL4_CapInitThreadPD);
	_core_cnode.move(initial_cspace, seL4_CapIRQControl); /* cannot be copied */
	_core_cnode.copy(initial_cspace, seL4_CapIOPort);
	_core_cnode.copy(initial_cspace, seL4_CapBootInfoFrame);
	_core_cnode.copy(initial_cspace, seL4_CapArchBootInfoFrame);
	_core_cnode.copy(initial_cspace, seL4_CapInitThreadIPCBuffer);
	_core_cnode.copy(initial_cspace, seL4_CapIPI);
	_core_cnode.copy(initial_cspace, seL4_CapDomain);

	/* replace seL4_CapInitThreadCNode with new top-level CNode */
	_core_cnode.copy(initial_cspace, Core_cspace::TOP_CNODE_SEL, seL4_CapInitThreadCNode);

	/* copy untyped memory selectors to core's CNode */
	seL4_BootInfo const &bi = sel4_boot_info();

	for (unsigned sel = bi.untyped.start; sel < bi.untyped.end; sel++)
		_core_cnode.copy(initial_cspace, sel);

	for (unsigned sel = bi.deviceUntyped.start; sel < bi.deviceUntyped.end; sel++)
		_core_cnode.copy(initial_cspace, sel);

	for (unsigned sel = bi.userImageFrames.start; sel < bi.userImageFrames.end; sel++)
		_core_cnode.copy(initial_cspace, sel);

	/* copy statically created CNode selectors to core's CNode */
	_core_cnode.copy(initial_cspace, Core_cspace::TOP_CNODE_SEL);
	_core_cnode.copy(initial_cspace, Core_cspace::CORE_PAD_CNODE_SEL);
	_core_cnode.copy(initial_cspace, Core_cspace::CORE_CNODE_SEL);
	_core_cnode.copy(initial_cspace, Core_cspace::PHYS_CNODE_SEL);

	/*
	 * Construct CNode hierarchy of core's CSpace
	 */

	/* insert 3rd-level core CNode into 2nd-level core-pad CNode */
	_core_pad_cnode.copy(initial_cspace, Core_cspace::CORE_CNODE_SEL, 0);

	/* insert 2nd-level core-pad CNode into 1st-level CNode */
	_top_cnode.copy(initial_cspace, Core_cspace::CORE_PAD_CNODE_SEL,
	                                Core_cspace::TOP_CNODE_CORE_IDX);

	/* insert 2nd-level phys-mem CNode into 1st-level CNode */
	_top_cnode.copy(initial_cspace, Core_cspace::PHYS_CNODE_SEL,
	                                Core_cspace::TOP_CNODE_PHYS_IDX);

	/* activate core's CSpace */
	{
		seL4_CapData_t null_data = { { 0 } };

		int const ret = seL4_TCB_SetSpace(seL4_CapInitThreadTCB,
		                                  seL4_CapNull, /* fault_ep */
		                                  Core_cspace::TOP_CNODE_SEL, null_data,
		                                  seL4_CapInitThreadPD, null_data);

		if (ret != 0) {
			PERR("%s: seL4_TCB_SetSpace returned %d", __FUNCTION__, ret);
		}
	}
}


void Platform::_init_core_page_table_registry()
{
	seL4_BootInfo const &bi = sel4_boot_info();

	/*
	 * Register initial page tables
	 */
	addr_t virt_addr = (addr_t)(&_prog_img_beg);
	for (unsigned sel = bi.userImagePTs.start; sel < bi.userImagePTs.end; sel++) {

		_core_page_table_registry.insert_page_table(virt_addr, sel);

		/* one page table has 1024 entries */
		virt_addr += 1024*get_page_size();
	}

	/*
	 * Register initial page frames
	 */
	virt_addr = (addr_t)(&_prog_img_beg);
	for (unsigned sel = bi.userImageFrames.start; sel < bi.userImageFrames.end; sel++) {

		_core_page_table_registry.insert_page_table_entry(virt_addr, sel);

		virt_addr += get_page_size();
	}
}


void Platform::_init_rom_modules()
{
	seL4_BootInfo const &bi = sel4_boot_info();

	/*
	 * Slab allocator for allocating 'Rom_module' meta data.
	 */
	static long slab_block[4096];
	static Tslab<Rom_module, sizeof(slab_block)>
		rom_module_slab(core_mem_alloc(), (Genode::Slab_block *)slab_block);;

	/*
	 * Allocate unused range of phys CNode address space where to make the
	 * boot modules available.
	 */
	void *out_ptr = nullptr;
	size_t const modules_size = (addr_t)&_boot_modules_binaries_end
	                          - (addr_t)&_boot_modules_binaries_begin + 1;

	Range_allocator::Alloc_return const alloc_ret =
		_unused_phys_alloc.alloc_aligned(modules_size, &out_ptr, get_page_size_log2());

	if (alloc_ret.is_error()) {
		PERR("could not reserve phys CNode space for boot modules");
		struct Init_rom_modules_failed { };
		throw Init_rom_modules_failed();
	}

	/*
	 * Calculate frame frame selector used to back the boot modules
	 */
	addr_t const unused_range_start      = (addr_t)out_ptr;
	addr_t const unused_first_frame_sel  = unused_range_start >> get_page_size_log2();
	addr_t const modules_start           = (addr_t)&_boot_modules_binaries_begin;
	addr_t const modules_core_offset     = modules_start
	                                     - (addr_t)&_prog_img_beg;
	addr_t const modules_first_frame_sel = bi.userImageFrames.start
	                                     + (modules_core_offset >> get_page_size_log2());

	Boot_module_header const *header = &_boot_modules_headers_begin;
	for (; header < &_boot_modules_headers_end; header++) {

		/* offset relative to first module */
		addr_t const module_offset        = header->base - modules_start;
		addr_t const module_offset_frames = module_offset >> get_page_size_log2();
		size_t const module_size          = round_page(header->size);
		addr_t const module_frame_sel     = modules_first_frame_sel
		                                  + module_offset_frames;
		size_t const module_num_frames    = module_size >> get_page_size_log2();

		/*
		 * Destination frame within phys CNode
		 */
		addr_t const dst_frame = unused_first_frame_sel + module_offset_frames;

		/*
		 * Install the module's frame selectors into phys CNode
		 */
		Cnode_base const initial_cspace(seL4_CapInitThreadCNode, 32);
		for (unsigned i = 0; i < module_num_frames; i++)
			_phys_cnode.copy(initial_cspace, module_frame_sel + i, dst_frame + i);

		PLOG("boot module '%s' (%zd bytes)", header->name, header->size);

		/*
		 * Register ROM module, the base address refers to location of the
		 * ROM module within the phys CNode address space.
		 */
		Rom_module * rom_module = new (rom_module_slab)
			Rom_module(dst_frame << get_page_size_log2(), header->size,
			           (const char*)header->name);

		_rom_fs.insert(rom_module);
	}
}


Platform::Platform()
:

	_io_mem_alloc(core_mem_alloc()), _io_port_alloc(core_mem_alloc()),
	_irq_alloc(core_mem_alloc()),
	_unused_phys_alloc(core_mem_alloc()),
	_init_unused_phys_alloc_done((_init_unused_phys_alloc(), true)),
	_vm_base(0x2000), /* 2nd page is used as IPC buffer of main thread */
	_vm_size(2*1024*1024*1024UL - _vm_base), /* use the lower 2GiB */
	_init_allocators_done((_init_allocators(), true)),
	_init_sel4_ipc_buffer_done((init_sel4_ipc_buffer(), true)),
	_switch_to_core_cspace_done((_switch_to_core_cspace(), true)),
	_core_page_table_registry(*core_mem_alloc()),
	_init_core_page_table_registry_done((_init_core_page_table_registry(), true)),
	_core_vm_space(seL4_CapInitThreadPD,
	               Core_cspace::CORE_VM_PAD_CNODE_SEL,
	               Core_cspace::CORE_VM_CNODE_SEL,
	               _phys_alloc,
	               _top_cnode,
	               _core_cnode,
	               _phys_cnode,
	               Core_cspace::CORE_VM_ID,
	               _core_page_table_registry)
{
	/*
	 * Print statistics about allocator initialization
	 */
	printf("VM area at [%08lx,%08lx)\n", _vm_base, _vm_base + _vm_size);

	if (verbose_boot_info) {
		printf(":phys_alloc:       "); _core_mem_alloc.phys_alloc()->raw()->dump_addr_tree();
		printf(":unused_phys_alloc:"); _unused_phys_alloc.raw()->dump_addr_tree();
		printf(":virt_alloc:       "); _core_mem_alloc.virt_alloc()->raw()->dump_addr_tree();
		printf(":io_mem_alloc:     "); _io_mem_alloc.raw()->dump_addr_tree();
	}

	_init_rom_modules();
}


unsigned Platform::alloc_core_sel()
{
	Lock::Guard guard(_core_sel_alloc_lock);

	return _core_sel_alloc.alloc();
}


unsigned Platform::alloc_core_rcv_sel()
{
	unsigned rcv_sel = alloc_core_sel();

	seL4_SetCapReceivePath(_core_cnode.sel(), rcv_sel, _core_cnode.size_log2());

	return rcv_sel;
}


void Platform::reset_sel(unsigned sel)
{
	_core_cnode.remove(sel);
}


void Platform::free_core_sel(unsigned sel)
{
	Lock::Guard guard(_core_sel_alloc_lock);

	_core_sel_alloc.free(sel);
}


void Platform::wait_for_exit()
{
	sleep_forever();
}


void Core_parent::exit(int exit_value) { }
