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
{ }


Platform_pd::~Platform_pd()
{
	platform_specific()->free_core_sel(_vm_cnode_sel);
	platform_specific()->free_core_sel(_vm_pad_cnode_sel);
	platform_specific()->free_core_sel(_cspace_cnode_sel);
}
