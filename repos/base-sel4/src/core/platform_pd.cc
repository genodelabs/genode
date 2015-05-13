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


void Platform_pd::map_ipc_buffer_of_initial_thread(addr_t ipc_buffer_phys)
{
	if (_initial_ipc_buffer_mapped)
		return;

	_vm_space.map(ipc_buffer_phys, INITIAL_IPC_BUFFER_VIRT, 1);

	_initial_ipc_buffer_mapped = true;
}


Platform_pd::Platform_pd(Allocator * md_alloc, size_t ram_quota,
                         char const *, signed pd_id, bool create)
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
	platform_specific()->free_core_sel(_vm_cnode_sel);
	platform_specific()->free_core_sel(_vm_pad_cnode_sel);
	platform_specific()->free_core_sel(_cspace_cnode_sel);
}
