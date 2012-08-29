/*
 * \brief  Platform interface implementation
 * \author Martin Stein
 * \date   2010-09-08
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/printf.h>
#include <core_parent.h>
#include <platform.h>
#include <map_local.h>
#include <kernel/syscalls.h>
#include <cpu/config.h>
#include <util/math.h>

static bool const verbose = 0;

extern unsigned _program_image_begin;
extern unsigned _program_image_end;

extern unsigned _boot_modules_meta_start;
extern unsigned _boot_modules_meta_end;

namespace Roottask
{
	/**
	 * Entry for the core-pager-thread that handles all
	 * pagefaults belonging to core-threads. Itself has
	 * to be paged 1:1 by the kernel. Core pager maps all
	 * 1:1 except of the thread-context-area
	 */
	static void pager();

	static Kernel::Utcb  pager_utcb;
	static Cpu::word_t   pager_stack[Cpu::_4KB_SIZE];
}


Genode::Thread_base::Context * Roottask::physical_context(Genode::Native_thread_id tid)
{
	using namespace Cpu;
	using Genode::Thread_base;

	static const unsigned int aligned_size = 
	Math::round_up<unsigned int>(CONTEXT_SIZE, 
	                               CONTEXT_PAGE_SIZE_LOG2);
	static Thread_base::Context * _context[User::MAX_THREAD_ID];

	if (tid >= sizeof(_context)/sizeof(_context[0])) {
		PERR("Native thread ID out of range");
		return 0;
	}

	if(!_context[tid]) {

		/* Allocate new context */
		if(!Genode::platform_specific()->
		    core_mem_alloc()->
		    alloc_aligned(aligned_size,
		                  (void**)&_context[tid],
		                  CONTEXT_PAGE_SIZE_LOG2))
		{
			PERR("Allocate memory for a new stack- and misc-area failed");
			return 0;
		}
		_context[tid] = (Thread_base::Context*)((addr_t)_context[tid] + 
		                aligned_size - sizeof(Thread_base::Context));

		/* Synchronize output of 'Genode::physical_utcb' if alignment fits */
		if(Math::round_up<addr_t>((addr_t)&_context[tid]->utcb, 
		                            Kernel::Utcb::ALIGNMENT_LOG2)!=
		   (addr_t)&_context[tid]->utcb)
		{
			PINF("%8X, %8X", (unsigned)Math::round_up<addr_t>((addr_t)&_context[tid]->utcb, 
			           Kernel::Utcb::ALIGNMENT_LOG2), (unsigned)&_context[tid]->utcb);
			           
			PWRN("Wrong UTCB alignment in context");
		} else {
			Genode::physical_utcb(tid, (Kernel::Utcb*)&_context[tid]->utcb);
		}
		if(verbose) {
			PDBG("Context %i: [%p|%p|%p|%p]", tid, 
			     (void*)((addr_t)_context[tid] + sizeof(Thread_base::Context) - aligned_size),
			     (Thread_base::Context*)((addr_t)_context[tid] - STACK_SIZE), 
			     _context[tid], &_context[tid]->utcb);
		}
	}
	return _context[tid];
}


void Roottask::pager()
{
	using namespace Genode;
	using namespace Roottask;

	typedef Platform_pd::Context_part     Context_part;
	typedef Kernel::Paging::Request       Request;
	typedef Kernel::Paging::Physical_page Physical_page;

	static Physical_page::size_t context_page_size;
	if(Physical_page::size_by_size_log2(context_page_size, CONTEXT_PAGE_SIZE_LOG2)){
		PERR("Invalid page size for thread context area");
	}

	Request *r = (Request*)&pager_utcb;

	while (1) {
		unsigned request_length = Kernel::ipc_serve(0);
		if (request_length != sizeof(Request)) {
			PERR("Invalid request");
			continue;
		}

		addr_t              pa = 0;

		Physical_page::size_t ps = Physical_page::INVALID_SIZE;
		addr_t              va = r->virtual_page.address();

		Native_thread_id context_owner = 0;
		Context_part     context_part  = Platform_pd::NO_CONTEXT_PART;
		unsigned         stack_offset  = 0;

		if (platform_pd()->metadata_if_context_address(va, &context_owner,
		                                               &context_part,
		                                               &stack_offset)) 
		{
			switch (context_part) {

			case Platform_pd::STACK_AREA:
				{
					Cpu::word_t* pstack = (Cpu::word_t*)physical_context(context_owner);
					pa = (addr_t)(pstack-(stack_offset/sizeof(Cpu::word_t)));
					break;
				}

			case Platform_pd::UTCB_AREA:
				pa = (addr_t)physical_utcb(context_owner);
				break;

			case Platform_pd::MISC_AREA:
				pa = (addr_t)physical_context(context_owner)->stack;
				break;

			default:
				PERR("No roottask mapping, "
				     "vaddr=0x%p, tid=%i, ip=%p\n",
				     (void*)r->virtual_page.address(),
				     r->source.tid,
				     (void*)r->source.ip);
				break;
			}
			ps = context_page_size;
		} else {
			pa = va;
			ps = Physical_page::MAX_VALID_SIZE;
		}

		Kernel::tlb_load(pa, va, r->virtual_page.protection_id(),
		                 ps, Physical_page::RWX);

		Kernel::thread_wake(r->source.tid);
	}
}


void Genode::Platform::_optimize_init_img_rom(long int & base, size_t const & size)
{
	enum {
		INIT_TEXT_SEGM_ALIGN_LOG2 = Cpu::_64KB_SIZE_LOG2,
		INIT_TEXT_SEGM_ALIGN      = 1 << INIT_TEXT_SEGM_ALIGN_LOG2,
		ELF_HEADER_SIZE           = Cpu::_4KB_SIZE
	};

	/* Preserve old location for now */
	long int const old_base = base;
	_core_mem_alloc.remove_range((addr_t)old_base, size);

	/* Search for location where text-segment would be mapable 
	 * with pages of size INIT_TEXT_SEGM_ALIGN */
	if (_core_mem_alloc.alloc_aligned(size + 2*INIT_TEXT_SEGM_ALIGN,
	                                  (void**)&base, INIT_TEXT_SEGM_ALIGN_LOG2)) 
	{
		/* Found better location so move */
		base = base + INIT_TEXT_SEGM_ALIGN - ELF_HEADER_SIZE;
		memcpy((void*)base, (void*)old_base, size);
		_core_mem_alloc.add_range((addr_t)old_base, size);
		return;
	}
	/* Keep old location */
	base = old_base;
}


Genode::Platform::Platform() :
	_core_mem_alloc(0),
	_io_mem_alloc(core_mem_alloc()), _io_port_alloc(core_mem_alloc()),
	_irq_alloc(core_mem_alloc()), _vm_base(0), _vm_size(0)
{

	using namespace Roottask;
	using namespace Genode;

	_core_mem_alloc.add_range((addr_t)Cpu::RAM_BASE, (size_t)Cpu::RAM_SIZE);

	/***************************************************
	 * Avoid allocations on '_core_mem_alloc' since it *
	 * contains space yet that is in use               *
	 ***************************************************/

	/* Preserve core's program image range with page-granularity from allocation */
	addr_t const img_base  = trunc_page((addr_t)&_program_image_begin);
	size_t const img_size  = round_page((addr_t)&_program_image_end) - img_base;
	_core_mem_alloc.remove_range(img_base, img_size);

	/* Preserve core's context area with page-granularity from allocation */
	addr_t const ctxt_area_base  = trunc_page((addr_t)Native_config::context_area_virtual_base());
	size_t const ctxt_area_size  = round_page((addr_t)Native_config::context_area_virtual_base());
	_core_mem_alloc.remove_range(ctxt_area_base, ctxt_area_size);

	/* Preserve UART MMIO with page-granularity from allocation */
	addr_t const uart_base = trunc_page(User::UART_BASE);
	_core_mem_alloc.remove_range(uart_base, get_page_size());

	/* Format of module meta-data as found in the ROM module image */
	struct Boot_module
	{
		long name;  /* physical address of null-terminated string */
		long base;  /* physical address of module data */
		long size;  /* size of module data in bytes */
	};

	addr_t const  md_base = (addr_t)&_boot_modules_meta_start;
	addr_t const  md_top  = (addr_t)&_boot_modules_meta_end;
	size_t const  meta_size = md_top - md_base;

	if (meta_size > get_page_size()) {
		PERR("Boot modules header is larger than supported");
		sleep_forever();
	}
	Boot_module * module = (Boot_module *)md_base;
	Boot_module * init_module=0;

	/* Preserve boot modules from allocation */
	for (; (addr_t)module < md_top; module++) {
		const char *name = (const char*)module->name;

		/* Init's module will need allocation because we optimize its location */
		if (!strcmp(name, "init")) 
		{
			init_module = module;
			continue;
		}
		_core_mem_alloc.remove_range(trunc_page(module->base), 
		                             round_page(module->size));
	}
	_optimize_init_img_rom(init_module->base, init_module->size);
	_core_mem_alloc.remove_range(trunc_page(init_module->base), 
	                             round_page(init_module->size));

	/*****************************************************************
	 * from now on it's save to allocate memory on '_core_mem_alloc' *
	 *****************************************************************/

	/* Initialize ROM FS with the given boot modules */
	module = (Boot_module *)md_base;
	for (; (addr_t)module < md_top; module++) {
		Rom_module *rom_module = new (core_mem_alloc())
			Rom_module(module->base, module->size, (const char*)module->name);
		_rom_fs.insert(rom_module);
	}

	/* Start the core-pager */
	if(Kernel::thread_create(PAGER_TID, PROTECTION_ID,
	                         Kernel::INVALID_THREAD_ID,
	                         &pager_utcb,
	                         (addr_t)pager, 
	                         (addr_t)&pager_stack[sizeof(pager_stack)/sizeof(pager_stack[0])],
	                         true << Kernel::THREAD_CREATE__PARAM__IS_ROOT_LSHIFT))
	{
		PERR("Couldn't start cores pager");
		sleep_forever();
	}

	/* Core's mainthread shall be paged by the core-pager */
	Kernel::thread_pager(MAIN_THREAD_ID, PAGER_TID);

	/* Initialze core's remaining allocators */
	_irq_alloc.add_range(User::MIN_IRQ, User::MAX_IRQ-User::MIN_IRQ);
	_io_mem_alloc.add_range(User::IO_MEM_BASE, User::IO_MEM_SIZE);

	/* Setup virtual memory for common programs */
	_vm_base=User::VADDR_BASE;
	_vm_size=User::VADDR_SIZE - get_page_size();

	if (verbose) {
		PINF("Printing core memory layout summary");
		printf("[_core_mem_alloc]\n"); _core_mem_alloc.raw()->dump_addr_tree();
		printf("[_io_mem_alloc]\n"); _io_mem_alloc.raw()->dump_addr_tree();
		printf("[_irq_alloc]\n"); _irq_alloc.raw()->dump_addr_tree();
	}
}

void Genode::Core_parent::exit(int exit_value) { }


