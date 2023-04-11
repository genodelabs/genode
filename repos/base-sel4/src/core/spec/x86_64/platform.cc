/*
 * \brief   Platform interface implementation - x86_64 specific
 * \author  Alexander Boettcher
 * \date    2017-07-05
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* base includes */
#include <base/internal/crt0.h>

/* core includes */
#include <boot_modules.h>
#include <platform.h>

#include <thread_sel4.h>
#include "arch_kernel_object.h"

using namespace Core;


seL4_Word Untyped_memory::smallest_page_type() { return seL4_X86_4K; }


void Platform::init_sel4_ipc_buffer()
{
	/*
	 * Setup tls pointer such, that it points to the (kernel created) core
	 * main thread IPC buffer. The fs register is used in seL4_GetIPCBuffer().
	 */
	seL4_BootInfo const &bi = sel4_boot_info();
	seL4_SetTLSBase((unsigned long)&bi.ipcBuffer);
}


long Platform::_unmap_page_frame(Cap_sel const &sel) {
	return seL4_X86_Page_Unmap(sel.value()); }


void Platform::_init_core_page_table_registry()
{
	seL4_BootInfo const &bi = sel4_boot_info();

	addr_t virt_addr = (addr_t)(&_prog_img_beg);
	unsigned sel     = (unsigned)bi.userImagePaging.start;

	/* we don't know the physical location of some objects XXX */
	enum { XXX_PHYS_UNKNOWN = ~0UL };

	/*
	 * Register initial pdpt and page directory
	 */
	_core_page_table_registry.insert_page_level3(virt_addr, Cap_sel(sel),
	                                             XXX_PHYS_UNKNOWN,
	                                             PAGE_PDPT_LOG2_SIZE);
	_core_page_table_registry.insert_page_directory(virt_addr,
	                                                Cap_sel(sel + 1),
	                                                XXX_PHYS_UNKNOWN,
	                                                PAGE_DIR_LOG2_SIZE);
	sel += 2;

	/*
	 * Register initial page tables
	 */
	for (; sel < bi.userImagePaging.end; sel++) {
		_core_page_table_registry.insert_page_table(virt_addr, Cap_sel(sel),
		                                            XXX_PHYS_UNKNOWN,
		                                            PAGE_TABLE_LOG2_SIZE);
		virt_addr += 512 * get_page_size();
	}

	/* initialize 16k memory allocator */
	phys_alloc_16k(&core_mem_alloc());

	/* reserve some memory for VCPUs - must be 16k */
	enum { MAX_VCPU_COUNT = 16 };
	addr_t const max_pd_mem = MAX_VCPU_COUNT * (1UL << Vcpu_kobj::SIZE_LOG2);

	_initial_untyped_pool.turn_into_untyped_object(Core_cspace::TOP_CNODE_UNTYPED_16K,
		[&] (addr_t const phys, addr_t const size, bool const device_memory) {

			if (device_memory)
				return false;

			phys_alloc_16k().add_range(phys, size);
			_unused_phys_alloc.remove_range(phys, size);

			return true;
		},
		Vcpu_kobj::SIZE_LOG2, max_pd_mem);

	log(":phys_mem_16k:     ",  phys_alloc_16k());
}


void Platform::_init_io_ports()
{
	enum { PORTS = 0x10000, PORT_FIRST = 0, PORT_LAST = PORTS - 1 };

	/* I/O port allocator (only meaningful for x86) */
	_io_port_alloc.add_range(PORT_FIRST, PORTS);

	/* create I/O port capability used by io_port_session_support.cc */
	auto const root   = _core_cnode.sel().value();
	auto const index  = Core_cspace::io_port_sel();
	auto const depth  = CONFIG_ROOT_CNODE_SIZE_BITS;

	auto const result = seL4_X86_IOPortControl_Issue(seL4_CapIOPortControl,
	                                                 PORT_FIRST, PORT_LAST,
	                                                 root, index, depth);
	if (result != 0)
		error("IO Port access not available");
}
