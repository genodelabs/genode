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
#include <kernel_object.h>

/* base-internal includes */
#include <base/internal/capability_space_sel4.h>

using namespace Core;


/*****************************************
 ** Allocator for protection-domain IDs **
 *****************************************/

struct Pd_id_alloc : Platform_pd::Pd_id_allocator
{
	Pd_id_alloc()
	{
		/*
		 * Reserve Top_cnode_index used by core
		 */
		bool ok = _reserve(Core_cspace::TOP_CNODE_CORE_IDX, 1); /* 0x000 */
		ASSERT (ok);
		ok = _reserve(Core_cspace::CORE_VM_ID             , 1); /* 0x001 */
		ASSERT (ok);
		ok = _reserve(Core_cspace::TOP_CNODE_UNTYPED_16K  , 1); /* 0x7fd */
		ASSERT (ok);
		ok = _reserve(Core_cspace::TOP_CNODE_UNTYPED_4K   , 1); /* 0x7fe */
		ASSERT (ok);
		ok = _reserve(Core_cspace::TOP_CNODE_PHYS_IDX     , 1); /* 0x7ff */
		ASSERT (ok);
	}
};


Platform_pd::Pd_id_allocator & Platform_pd::pd_id_alloc()
{
	static Pd_id_alloc inst;
	return inst;
}


bool Platform_pd::map_ipc_buffer(Ipc_buffer_phys const &from,
                                 Utcb_virt const to)
{
	if (!_vm_space.constructed())
		return false;

	return from.convert<bool>([&](auto &result) {
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

		if (!_vm_space->alloc_page_tables(to.addr, get_page_size()))
			return false;

		_vm_space->map(addr_t(result.ptr), to.addr, ONE_PAGE, attr);

		return true;
	}, [&] (auto) { return false; });
}


void Platform_pd::unmap_ipc_buffer(Utcb_virt const utcb)
{
	if (!_vm_space.constructed())
		return;

	_vm_space->unmap(utcb.addr, 1);
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
	if (_cspace_cnode_2nd[0].constructed() &&
	    _cspace_cnode_2nd[0]->constructed())
		_cspace_cnode_2nd[0]->copy(platform_specific().core_cnode(),
		                           Cnode_index(ipc_cap_data.sel),
		                           Cnode_index(INITIAL_SEL_PARENT));
}


bool Platform_pd::install_mapping(Mapping const &mapping,
                                  const char *thread_name)
{
	size_t const num_bytes = 1UL << mapping.size_log2;
	size_t const num_pages = num_bytes >> get_page_size_log2();

	if (!_vm_space.constructed())
		return false;

	if (!_vm_space->alloc_page_tables(mapping.dst_addr, num_bytes))
		return false;

	Vm_space::Map_attr const attr { .cached         = mapping.cached,
	                                .write_combined = mapping.write_combined,
	                                .writeable      = mapping.writeable,
	                                .executable     = mapping.executable,
	                                .flush_support  = true };

	if (_vm_space->map(mapping.src_addr, mapping.dst_addr, num_pages, attr))
		return true;

	warning("mapping failure for thread '", thread_name,
	        "' in pd '", _vm_space->name, "'");
	return false;
}


void Platform_pd::flush(addr_t virt_addr, size_t size, Core_local_addr)
{
	if (!_vm_space.constructed())
		return;

	_vm_space->unmap(virt_addr, round_page(size) >> get_page_size_log2());
}


Platform_pd::Platform_pd(Accounted_mapped_ram_allocator &,
                         Allocator &md_alloc, Name const &name)
:
	_page_table_registry(md_alloc)
{
	if (!_init_page_directory())
		return;

	if (!_page_directory_sel.value() || _page_directory.failed())
		return;

	platform_specific().core_sel_alloc().alloc().with_result([&](auto sel) {
		_cspace_cnode_1st.construct(platform_specific().core_cnode().sel(),
		                            Cap_sel(unsigned(sel)),
		                            CSPACE_SIZE_LOG2_1ST,
		                            platform().ram_alloc());
	}, [](auto) { });

	if (!_cspace_cnode_1st.constructed() || !_cspace_cnode_1st->constructed())
		return;

	pd_id_alloc().alloc().with_result(
		[&](auto idx) { _id = unsigned(idx); },
		[&](auto    ) { /* checked below */ });

	if (!_id)
		return;

	_vm_space.construct(_page_directory_sel,
	                    platform_specific().core_sel_alloc(),
	                    platform().ram_alloc(),
	                    platform_specific().top_cnode(),
	                    platform_specific().core_cnode(),
	                    platform_specific().phys_cnode(),
	                    _id,
	                    _page_table_registry,
	                    name);

	if (!_vm_space->constructed())
		return;

	for (unsigned i = 0; i < sizeof(_cspace_cnode_2nd) /
	                         sizeof(_cspace_cnode_2nd[0]); i++) {

		platform_specific().core_sel_alloc().alloc().with_result([&](auto sel) {
			/* add all 2nd level CSpace's to 1st level CSpace */
			_cspace_cnode_2nd[i].construct(platform_specific().core_cnode().sel(),
			                               Cap_sel(unsigned(sel)),
			                               CSPACE_SIZE_LOG2_2ND,
			                               platform().ram_alloc());

			if (!_cspace_cnode_2nd->constructed())
				return;

			_cspace_cnode_1st->copy(platform_specific().core_cnode(),
			                        _cspace_cnode_2nd[i]->sel(),
			                        Cnode_index(i));
		}, [](auto) { /* _cspace_cnode_2nd[0] stays invalid, which is checked for */  });
	}

	/* install CSpace selector at predefined position in the PD's CSpace */
	if (_cspace_cnode_2nd[0].constructed() && _cspace_cnode_2nd[0]->constructed())
		_cspace_cnode_2nd[0]->copy(platform_specific().core_cnode(),
		                           _cspace_cnode_1st->sel(),
		                           Cnode_index(INITIAL_SEL_CNODE));
}


Platform_pd::~Platform_pd()
{
	with_cspace_cnode_1st([&](auto &cspace_cnode_1st) {

		for (unsigned i = 0; i < sizeof(_cspace_cnode_2nd) /
		                         sizeof(_cspace_cnode_2nd[0]); i++) {
			cspace_cnode_1st.remove(Cnode_index(i));

			if (_cspace_cnode_2nd[i].constructed()) {
				_cspace_cnode_2nd[i]->destruct(platform().ram_alloc(), true);
				platform_specific().core_sel_alloc().free(_cspace_cnode_2nd[i]->sel());
			}
		}

		cspace_cnode_1st.destruct(platform().ram_alloc(), true);
		platform_specific().core_sel_alloc().free(cspace_cnode_1st.sel());
	});

	_cspace_cnode_1st.destruct();

	_deinit_page_directory();

	if (_page_directory_sel.value())
		platform_specific().core_sel_alloc().free(_page_directory_sel);

	if (_id)
		pd_id_alloc().free(_id);
}
