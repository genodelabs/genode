/*
 * \brief   Platform interface implementation - ARM specific
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
#include <platform.h>
#include <platform_pd.h>
#include <assertion.h>

#include "arch_kernel_object.h"

using namespace Core;


static Phys_allocator *_phys_alloc_16k_ptr;


static Phys_allocator &phys_alloc_16k()
{
	if (_phys_alloc_16k_ptr)
		return *_phys_alloc_16k_ptr;

	ASSERT_NEVER_CALLED;
}


seL4_Word Untyped_memory::smallest_page_type() {
	return seL4_ARM_SmallPageObject; }


void Platform::init_sel4_ipc_buffer()
{
	/*
	 * Setup tls pointer such, that it points to the (kernel created) core
	 * main thread IPC buffer. It is used in seL4_GetIPCBuffer().
	 */
	seL4_BootInfo const &bi = sel4_boot_info();
	seL4_SetTLSBase((unsigned long)&bi.ipcBuffer);
}


long Platform::_unmap_page_frame(Cap_sel const &sel) {
	return seL4_ARM_Page_Unmap(sel.value()); }


void Platform::_init_core_page_table_registry()
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
		virt_addr += 256 * get_page_size();
	}

	/* initialize 16k memory allocator */
	{
		static Phys_allocator inst(&core_mem_alloc());
		_phys_alloc_16k_ptr = &inst;
	}
	
	/* reserve some memory for page directory construction - must be 16k on ARM */
	enum { MAX_PROCESS_COUNT = 32 };
	addr_t const max_pd_mem = MAX_PROCESS_COUNT * (1UL << Page_directory_kobj::SIZE_LOG2);

	_initial_untyped_pool.turn_into_untyped_object(Core_cspace::TOP_CNODE_UNTYPED_16K,
		[&] (addr_t const phys, addr_t const size, bool const device_memory) {

			if (device_memory)
				return false;

			if (phys_alloc_16k()  .add_range   (phys, size).failed()
			 || _unused_phys_alloc.remove_range(phys, size).failed())
				warning("unable to register range as RAM: ", Hex_range(phys, size));

			return true;
		},
		Page_directory_kobj::SIZE_LOG2, max_pd_mem);

	log(":phys_mem_16k:     ",  phys_alloc_16k());
}


Cap_sel Platform_pd::_init_page_directory()
{
	Cap_sel sel_page_directory = platform_specific().core_sel_alloc().alloc();

	/* page directory table contains 4096 elements of 32bits -> 16k required */
	enum { PAGES_16K = (1UL << Page_directory_kobj::SIZE_LOG2) / 4096 };

	_page_directory = Untyped_memory::alloc_pages(phys_alloc_16k(), PAGES_16K);

	_page_directory.with_result([&](auto &result) {
		auto const service = Untyped_memory::_core_local_sel(Core_cspace::TOP_CNODE_UNTYPED_16K, addr_t(result.ptr), Page_directory_kobj::SIZE_LOG2).value();

		create<Page_directory_kobj>(service,
		                            platform_specific().core_cnode().sel(),
		                            sel_page_directory);


		long ret = seL4_ARM_ASIDPool_Assign(platform_specific().asid_pool().value(),
		                                    sel_page_directory.value());

		if (ret != seL4_NoError)
			error("seL4_ARM_ASIDPool_Assign returned ", ret);

	}, [&] (auto) { /* handled manually in platform_pd - to be improved */ });

	return sel_page_directory;
}


void Platform_pd::_deinit_page_directory()
{
	_page_directory.with_result([&](auto &result) {
		int ret = seL4_CNode_Delete(seL4_CapInitThreadCNode,
		                            _page_directory_sel.value(), 32);
		if (ret != seL4_NoError) {
			error(__FUNCTION__, ": could not free ASID entry, "
			      "leaking physical memory ", ret);
			result.deallocate = false;
		}
	}, [](auto) { /* allocation failed, so we have nothing to revert */ });
}


void Platform::_init_io_ports()
{
}
