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
#include <platform.h>
#include <util.h>
#include <core_cspace.h>
#include <kernel_object.h>

/* base-internal includes */
#include <base/internal/capability_space_sel4.h>

using namespace Core;


/*****************************************
 ** Allocator for protection-domain IDs **
 *****************************************/

struct Pd_id_alloc : Bit_allocator<1024>
{
	Pd_id_alloc()
	{
		/*
		 * Skip 0 because this top-level index is used to address the core
		 * CNode.
		 */
		_reserve(0, 1);
		_reserve(Core_cspace::CORE_VM_ID, 1);
	}
};


Bit_allocator<1024> &Platform_pd::pd_id_alloc()
{
	static Pd_id_alloc inst;
	return inst;
}


void Platform_pd::map_ipc_buffer(Ipc_buffer_phys const from, Utcb_virt const to)
{
	/*
	 * Map IPC buffer
	 *
	 * XXX The mapping of the IPC buffer could be evicted from the PD's
	 *     'Vm_space'. In contrast to mapping that are created as a result of
	 *     the RM-session's page-fault resolution, the IPC buffer's mapping
	 *     won't be recoverable once flushed. For this reason, it is important
	 *     to attach the UTCB as a dataspace to the stack area to make the RM
	 *     session aware to the mapping. This code is missing.
	 */
	Vm_space::Map_attr const attr { .cached         = true,
	                                .write_combined = false,
	                                .writeable      = true,
	                                .executable     = false,
	                                .flush_support  = true };
	enum { ONE_PAGE = 1 };
	_vm_space.alloc_page_tables(to.addr, get_page_size());
	_vm_space.map(from.addr, to.addr, ONE_PAGE, attr);
}


void Platform_pd::unmap_ipc_buffer(Utcb_virt const utcb)
{
	_vm_space.unmap(utcb.addr, 1);
}


void Platform_pd::assign_parent(Native_capability parent)
{
	Capability_space::Ipc_cap_data const ipc_cap_data =
		Capability_space::ipc_cap_data(parent);

	_parent = parent;

	/*
	 * Install parent endpoint selector at the predefined position
	 * INITIAL_SEL_PARENT within the PD's CSpace.
	 */
	_cspace_cnode_2nd[0]->copy(platform_specific().core_cnode(),
	                           Cnode_index(ipc_cap_data.sel),
	                           Cnode_index(INITIAL_SEL_PARENT));
}


Cap_sel Platform_pd::alloc_sel()
{
	Mutex::Guard guard(_sel_alloc_mutex);

	return Cap_sel((uint32_t)_sel_alloc.alloc());
}


void Platform_pd::free_sel(Cap_sel sel)
{
	Mutex::Guard guard(_sel_alloc_mutex);

	_sel_alloc.free(sel.value());
}


bool Platform_pd::install_mapping(Mapping const &mapping,
                                  const char *thread_name)
{
	size_t const num_bytes = 1UL << mapping.size_log2;
	size_t const num_pages = num_bytes >> get_page_size_log2();

	_vm_space.alloc_page_tables(mapping.dst_addr, num_bytes);

	Vm_space::Map_attr const attr { .cached         = mapping.cached,
	                                .write_combined = mapping.write_combined,
	                                .writeable      = mapping.writeable,
	                                .executable     = mapping.executable,
	                                .flush_support  = true };

	if (_vm_space.map(mapping.src_addr, mapping.dst_addr, num_pages, attr))
		return true;

	warning("mapping failure for thread '", thread_name,
	        "' in pd '", _vm_space.pd_label(), "'");
	return false;
}


void Platform_pd::flush(addr_t virt_addr, size_t size, Core_local_addr)
{
	_vm_space.unmap(virt_addr, round_page(size) >> get_page_size_log2());
}


Platform_pd::Platform_pd(Allocator &md_alloc, char const *label)
:
	_id((uint32_t)pd_id_alloc().alloc()),
	_page_table_registry(md_alloc),
	_page_directory_sel(platform_specific().core_sel_alloc().alloc()),
	_page_directory(_init_page_directory()),
	_vm_space(_page_directory_sel,
	          platform_specific().core_sel_alloc(),
	          platform().ram_alloc(),
	          platform_specific().top_cnode(),
	          platform_specific().core_cnode(),
	          platform_specific().phys_cnode(),
	          _id,
	          _page_table_registry,
	          label),
	_cspace_cnode_1st(platform_specific().core_cnode().sel(),
	                  platform_specific().core_sel_alloc().alloc(),
	                  CSPACE_SIZE_LOG2_1ST,
	                  platform().ram_alloc())
{
	/* add all 2nd level CSpace's to 1st level CSpace */
	for (unsigned i = 0; i < sizeof(_cspace_cnode_2nd) /
	                         sizeof(_cspace_cnode_2nd[0]); i++) {
		_cspace_cnode_2nd[i].construct(platform_specific().core_cnode().sel(),
		                               platform_specific().core_sel_alloc().alloc(),
		                               CSPACE_SIZE_LOG2_2ND,
		                               platform().ram_alloc());

		_cspace_cnode_1st.copy(platform_specific().core_cnode(),
		                       _cspace_cnode_2nd[i]->sel(),
		                       Cnode_index(i));
	}

	/* install CSpace selector at predefined position in the PD's CSpace */
	_cspace_cnode_2nd[0]->copy(platform_specific().core_cnode(),
	                           _cspace_cnode_1st.sel(),
	                           Cnode_index(INITIAL_SEL_CNODE));
}


Platform_pd::~Platform_pd()
{
	for (unsigned i = 0; i < sizeof(_cspace_cnode_2nd) /
	                         sizeof(_cspace_cnode_2nd[0]); i++) {
		_cspace_cnode_1st.remove(Cnode_index(i));
		_cspace_cnode_2nd[i]->destruct(platform().ram_alloc(), true);
		platform_specific().core_sel_alloc().free(_cspace_cnode_2nd[i]->sel());
	}

	_cspace_cnode_1st.destruct(platform().ram_alloc(), true);
	platform_specific().core_sel_alloc().free(_cspace_cnode_1st.sel());

	_deinit_page_directory(_page_directory);
	platform_specific().core_sel_alloc().free(_page_directory_sel);
}
