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

/* Genode includes */
#include <base/log.h>

/* core includes */
#include <platform_pd.h>
#include <platform.h>
#include <util.h>
#include <core_cspace.h>
#include <kernel_object.h>

/* base-internal includes */
#include <base/internal/capability_space_sel4.h>

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


bool Platform_pd::bind_thread(Platform_thread *thread)
{
	ASSERT(thread);

	try {
		/* allocate fault handler selector in the PD's CSpace */
		thread->_fault_handler_sel = alloc_sel();
		/* allocate endpoint selector in the PD's CSpace */
		thread->_ep_sel = alloc_sel();
		/* allocate asynchronous selector used for locks in the PD's CSpace */
		thread->_lock_sel = thread->_utcb ? alloc_sel() : Cap_sel(INITIAL_SEL_LOCK);
	} catch (Platform_pd::Sel_bit_alloc::Out_of_indices) {
		if (thread->_fault_handler_sel.value()) {
			free_sel(thread->_fault_handler_sel);
			thread->_fault_handler_sel = Cap_sel(0);
		}
		if (thread->_ep_sel.value()) {
			free_sel(thread->_ep_sel);
			thread->_ep_sel = Cap_sel(0);
		}
		return false;
	}

	thread->_pd = this;

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
	addr_t const utcb = (thread->_utcb) ? thread->_utcb : thread->INITIAL_IPC_BUFFER_VIRT;

	enum { WRITABLE = true, ONE_PAGE = 1, FLUSHABLE = true, NON_EXECUTABLE = false };
	_vm_space.alloc_page_tables(utcb, get_page_size());
	_vm_space.map(thread->_info.ipc_buffer_phys, utcb, ONE_PAGE,
	              Cache_attribute::CACHED, WRITABLE, NON_EXECUTABLE, FLUSHABLE);
	return true;
}


void Platform_pd::unbind_thread(Platform_thread *thread)
{
	if (!thread)
		return;

	if (thread->_utcb)
		free_sel(thread->_lock_sel);
	free_sel(thread->_fault_handler_sel);
	free_sel(thread->_ep_sel);

	if (thread->_utcb)
		_vm_space.unmap(thread->_utcb, 1);
	else
		_vm_space.unmap(thread->INITIAL_IPC_BUFFER_VIRT, 1);
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
	_cspace_cnode_2nd[0]->copy(platform_specific()->core_cnode(),
	                           Cnode_index(ipc_cap_data.sel),
	                           Cnode_index(INITIAL_SEL_PARENT));
}


Cap_sel Platform_pd::alloc_sel()
{
	Lock::Guard guard(_sel_alloc_lock);

	return Cap_sel(_sel_alloc.alloc());
}


void Platform_pd::free_sel(Cap_sel sel)
{
	Lock::Guard guard(_sel_alloc_lock);

	_sel_alloc.free(sel.value());
}


bool Platform_pd::install_mapping(Mapping const &mapping,
                                  const char *thread_name)
{
	enum { FLUSHABLE = true };

	try {
		if (mapping.fault_type() != seL4_Fault_VMFault)
			throw 1;

		_vm_space.alloc_page_tables(mapping.to_virt(),
		                            mapping.num_pages() * get_page_size());

		_vm_space.map(mapping.from_phys(), mapping.to_virt(),
		              mapping.num_pages(), mapping.cacheability(),
		              mapping.writeable(), mapping.executable(), FLUSHABLE);
		return true;
	} catch (...) {
		char const * fault_name = "unknown";

		switch (mapping.fault_type()) {
		case seL4_Fault_NullFault:
			fault_name = "seL4_Fault_NullFault";
			break;
		case seL4_Fault_CapFault:
			fault_name = "seL4_Fault_CapFault";
			break;
		case seL4_Fault_UnknownSyscall:
			fault_name = "seL4_Fault_UnknownSyscall";
			break;
		case seL4_Fault_UserException:
			fault_name = "seL4_Fault_UserException";
			break;
		case seL4_Fault_VMFault:
			fault_name = "seL4_Fault_VMFault";
			break;
		}

		/* pager ep would die when we re-throw - let core survive */
		Genode::error("unexpected exception during fault '", fault_name, "'",
		              " - thread '", thread_name, "' in pd '",
		              _vm_space.pd_label(),"' stopped");
		return false;
	}
}


void Platform_pd::flush(addr_t virt_addr, size_t size, Core_local_addr)
{
	_vm_space.unmap(virt_addr, round_page(size) >> get_page_size_log2());
}


Platform_pd::Platform_pd(Allocator * md_alloc, char const *label,
                         signed pd_id, bool create)
:
	_id(pd_id_alloc().alloc()),
	_page_table_registry(*md_alloc),
	_page_directory_sel(platform_specific()->core_sel_alloc().alloc()),
	_page_directory(_init_page_directory()),
	_vm_space(_page_directory_sel,
	          platform_specific()->core_sel_alloc(),
	          *platform()->ram_alloc(),
	          platform_specific()->top_cnode(),
	          platform_specific()->core_cnode(),
	          platform_specific()->phys_cnode(),
	          _id,
	          _page_table_registry,
	          label),
	_cspace_cnode_1st(platform_specific()->core_cnode().sel(),
	                  platform_specific()->core_sel_alloc().alloc(),
	                  CSPACE_SIZE_LOG2_1ST,
	                  *platform()->ram_alloc())
{
	/* add all 2nd level CSpace's to 1st level CSpace */
	for (unsigned i = 0; i < sizeof(_cspace_cnode_2nd) /
	                         sizeof(_cspace_cnode_2nd[0]); i++) {
		_cspace_cnode_2nd[i].construct(platform_specific()->core_cnode().sel(),
		                               platform_specific()->core_sel_alloc().alloc(),
		                               CSPACE_SIZE_LOG2_2ND,
		                               *platform()->ram_alloc());

		_cspace_cnode_1st.copy(platform_specific()->core_cnode(),
		                       _cspace_cnode_2nd[i]->sel(),
		                       Cnode_index(i));
	}

	/* install CSpace selector at predefined position in the PD's CSpace */
	_cspace_cnode_2nd[0]->copy(platform_specific()->core_cnode(),
	                           _cspace_cnode_1st.sel(),
	                           Cnode_index(INITIAL_SEL_CNODE));
}


Platform_pd::~Platform_pd()
{
	for (unsigned i = 0; i < sizeof(_cspace_cnode_2nd) /
	                         sizeof(_cspace_cnode_2nd[0]); i++) {
		_cspace_cnode_1st.remove(Cnode_index(i));
		_cspace_cnode_2nd[i]->destruct(*platform()->ram_alloc(), true);
		platform_specific()->core_sel_alloc().free(_cspace_cnode_2nd[i]->sel());
	}

	_cspace_cnode_1st.destruct(*platform()->ram_alloc(), true);
	platform_specific()->core_sel_alloc().free(_cspace_cnode_1st.sel());

	_deinit_page_directory(_page_directory);
	platform_specific()->core_sel_alloc().free(_page_directory_sel);
}
