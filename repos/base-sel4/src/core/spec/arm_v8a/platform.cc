/*
 * \brief   Platform interface implementation - ARM specific
 * \author  Alexander Boettcher
 * \date    2017-07-05
 */

/*
 * Copyright (C) 2017-2025 Genode Labs GmbH
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


static Phys_allocator *_phys_alloc_8k_ptr;


static Phys_allocator &phys_alloc_8k()
{
	if (_phys_alloc_8k_ptr)
		return *_phys_alloc_8k_ptr;

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
	unsigned sel     = unsigned(bi.userImagePaging.start);

	/* we don't know the physical location of some objects XXX */
	enum { XXX_PHYS_UNKNOWN = ~0UL };

	if (_core_page_table_registry.insert_page_directory(virt_addr,
	                                                    Cap_sel(sel++),
	                                                    XXX_PHYS_UNKNOWN,
	                                                    PAGE_DIR_LOG2_SIZE).failed())
		error(__func__, ":", __LINE__, " page table allocation failed");

	/*
	 * Register initial page tables
	 */
	for (; sel < bi.userImagePaging.end; sel++) {
		if (_core_page_table_registry.insert_page_table(virt_addr, Cap_sel(sel),
		                                                XXX_PHYS_UNKNOWN,
		                                                PAGE_TABLE_LOG2_SIZE).failed())
			error("page table insertion failed");

		virt_addr += 512 * get_page_size();
	}

	/* initialize 8k memory allocator */
	{
		static Phys_allocator inst(&core_mem_alloc());
		_phys_alloc_8k_ptr = &inst;
	}

	/* reserve some memory for page directory construction - must be 8k on v8 */
	enum { MAX_PROCESS_COUNT = 64 };
	addr_t const max_pd_mem = MAX_PROCESS_COUNT * (1UL << Page_directory_kobj::SIZE_LOG2);

	_initial_untyped_pool.turn_into_untyped_object(Core_cspace::TOP_CNODE_UNTYPED_8K,
		[&] (addr_t const phys, addr_t const size, bool const device_memory) {

			if (device_memory)
				return false;

			if (_unused_phys_alloc.remove_range(phys, size).failed()) {
				warning("unable to register range as RAM: ", Hex_range(phys, size));
				return false;
			}

			if (phys_alloc_8k().add_range(phys, size).failed()) {
				if (_unused_phys_alloc.add_range(phys, size).failed())
					warning("unable to remove range as RAM: ", Hex_range(phys, size));
				warning("unable to register range as RAM: ", Hex_range(phys, size));
				return false;
			}

			return true;
		},
		[&] (addr_t const phys, addr_t const size, bool const device_memory) {
			if (device_memory)
				return;

			if (phys_alloc_8k()  .remove_range(phys, size).failed() ||
			    _unused_phys_alloc.add_range   (phys, size).failed())
				warning("unable to re-add phys RAM: ", Hex_range(phys, size));
		},
		Page_directory_kobj::SIZE_LOG2, max_pd_mem);

	log(":phys_mem_8k:     ",  phys_alloc_8k());
}


bool Platform_pd::_init_page_directory()
{
	return platform_specific().core_sel_alloc().alloc().convert<bool>([&](auto sel) {
		_page_directory_sel = Cap_sel(unsigned(sel));

		/* page directory table contains 8192 elements on 64bits */
		enum { PAGES_8K = (1UL << Page_directory_kobj::SIZE_LOG2) / 4096 };

		_page_directory = Untyped_memory::alloc_pages(phys_alloc_8k(), PAGES_8K);

		return _page_directory.convert<bool>([&](auto &result) {
			auto const service = Untyped_memory::_core_local_sel(Core_cspace::TOP_CNODE_UNTYPED_8K, addr_t(result.ptr), Page_directory_kobj::SIZE_LOG2).value();

			if (!create<Vspace_kobj>(service,
			                         platform_specific().core_cnode().sel(),
			                         _page_directory_sel))
				return false;

			long ret = seL4_ARM_ASIDPool_Assign(platform_specific().asid_pool().value(),
			                                    _page_directory_sel.value());

			if (ret != seL4_NoError)
				error("seL4_ARM_ASIDPool_Assign returned ", ret);

			return ret == seL4_NoError;

		}, [&] (auto) { return false; });
	}, [](auto) { return false; });
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
