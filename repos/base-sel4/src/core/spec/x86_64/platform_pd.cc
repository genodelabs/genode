/*
 * \brief   Protection-domain facility
 * \author  Norman Feske
 * \author  Alexander Boettcher
 * \date    2015-05-01
 */

/*
 * Copyright (C) 2015-2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <platform_pd.h>

#include "arch_kernel_object.h"

using namespace Core;


bool Platform_pd::_init_page_directory()
{
	return platform_specific().core_sel_alloc().alloc().convert<bool>([&](auto sel) {
		_page_directory_sel = Cap_sel(unsigned(sel));

		_page_directory = Untyped_memory::alloc_page(platform().ram_alloc());

		return _page_directory.convert<bool>([&](auto &result) {
			seL4_Untyped const service = Untyped_memory::untyped_sel(addr_t(result.ptr)).value();

			if (!create<Page_map_kobj>(service,
			                           platform_specific().core_cnode().sel(),
			                           _page_directory_sel))
				return false;

			long ret = seL4_X86_ASIDPool_Assign(platform_specific().asid_pool().value(),
			                                    _page_directory_sel.value());

			if (ret != seL4_NoError)
				error("seL4_X86_ASIDPool_Assign returned ", ret);

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
