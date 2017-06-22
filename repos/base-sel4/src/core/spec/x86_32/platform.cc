/*
 * \brief   Platform interface implementation - x86_32 specific
 * \author  Norman Feske
 * \date    2015-05-01
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* base includes */
#include <base/internal/crt0.h>

/* core includes */
#include <boot_modules.h>
#include <platform.h>

#include "arch_kernel_object.h"

seL4_Word Genode::Untyped_memory::smallest_page_type() { return seL4_X86_4K; }

void Genode::Platform::init_sel4_ipc_buffer()
{
	asm volatile ("movl %0, %%fs" :: "r"(IPCBUF_GDT_SELECTOR) : "memory");
}

long Genode::Platform::_unmap_page_frame(Cap_sel const &sel) {
	return seL4_X86_Page_Unmap(sel.value()); }

void Genode::Platform::_init_core_page_table_registry()
{
	seL4_BootInfo const &bi = sel4_boot_info();

	addr_t virt_addr = (addr_t)(&_prog_img_beg);
	unsigned sel     = bi.userImagePaging.start;

	/* we don't know the physical location of some objects XXX */
	enum { XXX_PHYS_UNKNOWN = ~0UL };

	/*
	 * Register initial page tables
	 */
	for (; sel < bi.userImagePaging.end; sel++) {
		_core_page_table_registry.insert_page_table(virt_addr, Cap_sel(sel),
		                                            XXX_PHYS_UNKNOWN,
		                                            PAGE_TABLE_LOG2_SIZE);
		virt_addr += 1024 * get_page_size();
	}

	/*
	 * Register initial page frames
	 * - actually we don't use them in core -> skip
	 */
#if 0
	addr_t const modules_start = reinterpret_cast<addr_t>(&_boot_modules_binaries_begin);
	addr_t const modules_end   = reinterpret_cast<addr_t>(&_boot_modules_binaries_end);

	virt_addr = (addr_t)(&_prog_img_beg);
	for (unsigned sel = bi.userImageFrames.start;
	     sel < bi.userImageFrames.end;
	     sel++, virt_addr += get_page_size()) {
		/* skip boot modules */
		if (modules_start <= virt_addr && virt_addr <= modules_end)
			continue;

		_core_page_table_registry.insert_page_table_entry(virt_addr, sel);
	}
#endif
}


