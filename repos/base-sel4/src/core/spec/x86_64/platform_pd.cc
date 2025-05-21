/*
 * \brief   Protection-domain facility
 * \author  Norman Feske
 * \date    2015-05-01
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <platform_pd.h>

#include "arch_kernel_object.h"

using namespace Core;


Cap_sel Platform_pd::_init_page_directory()
{
	Cap_sel sel_page_directory = platform_specific().core_sel_alloc().alloc();

	_page_directory = Untyped_memory::alloc_page(platform().ram_alloc());

	_page_directory.with_result([&](auto &result) {
		seL4_Untyped const service = Untyped_memory::untyped_sel(addr_t(result.ptr)).value();

		create<Page_map_kobj>(service, platform_specific().core_cnode().sel(),
		                      sel_page_directory);

		long ret = seL4_X86_ASIDPool_Assign(platform_specific().asid_pool().value(),
		                                    sel_page_directory.value());

		if (ret != seL4_NoError)
			error("seL4_X86_ASIDPool_Assign returned ", ret);

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
