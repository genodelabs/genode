/*
 * \brief   Protection-domain facility
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

/* core includes */
#include <platform_pd.h>
#include <platform.h>
#include <util.h>
#include <core_cspace.h>
#include <kernel_object.h>

/* base-internal includes */
#include <internal/capability_space_sel4.h>

using namespace Genode;


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


static Pd_id_alloc &pd_id_alloc()
{
	static Pd_id_alloc inst;
	return inst;
}


int Platform_pd::bind_thread(Platform_thread *thread)
{
	ASSERT(thread);

	thread->_pd = this;

	/*
	 * Map IPC buffer
	 *
	 * XXX The mapping of the IPC buffer could be evicted from the PD's
	 *     'Vm_space'. In contrast to mapping that are created as a result of
	 *     the RM-session's page-fault resolution, the IPC buffer's mapping
	 *     won't be recoverable once flushed. For this reason, it is important
	 *     to attach the UTCB as a dataspace to the context-area to make the RM
	 *     session aware to the mapping. This code is missing.
	 */
	if (thread->_utcb) {
		_vm_space.map(thread->_info.ipc_buffer_phys, thread->_utcb, 1);
	} else {
		_vm_space.map(thread->_info.ipc_buffer_phys, thread->INITIAL_IPC_BUFFER_VIRT, 1);
	}

	return 0;
}


void Platform_pd::unbind_thread(Platform_thread *thread)
{
	PDBG("not implemented");
}


int Platform_pd::assign_parent(Native_capability parent)
{
	Capability_space::Ipc_cap_data const ipc_cap_data =
		Capability_space::ipc_cap_data(parent);

	_parent = parent;

	/*
	 * Install parent endpoint selector at the predefined position
	 * INITIAL_SEL_PARENT within the PD's CSpace.
	 */
	_cspace_cnode.copy(platform_specific()->core_cnode(),
	                   ipc_cap_data.sel,
	                   INITIAL_SEL_PARENT);
	return 0;
}


Untyped_address Platform_pd::_init_page_directory()
{
	using namespace Kernel_object;
	return create<Page_directory>(*platform()->ram_alloc(),
	                              platform_specific()->core_cnode().sel(),
	                              _page_directory_sel);
}


unsigned Platform_pd::alloc_sel()
{
	Lock::Guard guard(_sel_alloc_lock);

	return _sel_alloc.alloc();
}


void Platform_pd::free_sel(unsigned sel)
{
	Lock::Guard guard(_sel_alloc_lock);

	_sel_alloc.free(sel);
}


void Platform_pd::install_mapping(Mapping const &mapping)
{
	_vm_space.map(mapping.from_phys(), mapping.to_virt(), mapping.num_pages());
}


void Platform_pd::flush(addr_t virt_addr, size_t size)
{
	_vm_space.unmap(virt_addr, round_page(size) >> get_page_size_log2());
}


Platform_pd::Platform_pd(Allocator * md_alloc, char const *,
                         signed pd_id, bool create)
:
	_id(pd_id_alloc().alloc()),
	_page_table_registry(*md_alloc),
	_vm_pad_cnode_sel(platform_specific()->alloc_core_sel()),
	_vm_cnode_sel(platform_specific()->alloc_core_sel()),
	_page_directory_sel(platform_specific()->alloc_core_sel()),
	_page_directory(_init_page_directory()),
	_vm_space(_page_directory_sel,
	          _vm_pad_cnode_sel, _vm_cnode_sel,
	          *platform()->ram_alloc(),
	          platform_specific()->top_cnode(),
	          platform_specific()->core_cnode(),
	          platform_specific()->phys_cnode(),
	          _id,
	          _page_table_registry),
	_cspace_cnode_sel(platform_specific()->alloc_core_sel()),
	_cspace_cnode(platform_specific()->core_cnode().sel(), _cspace_cnode_sel,
	              CSPACE_SIZE_LOG2,
	              *platform()->ram_alloc())
{
	/* install CSpace selector at predefined position in the PD's CSpace */
	_cspace_cnode.copy(platform_specific()->core_cnode(),
	                   _cspace_cnode_sel,
	                   INITIAL_SEL_CNODE);
}


Platform_pd::~Platform_pd()
{
	/* invalidate weak pointers to this object */
	Address_space::lock_for_destruction();

	platform_specific()->free_core_sel(_vm_cnode_sel);
	platform_specific()->free_core_sel(_vm_pad_cnode_sel);
	platform_specific()->free_core_sel(_cspace_cnode_sel);
}
