/*
 * \brief  VM session component for 'base-hw'
 * \author Stefan Kalkowski
 * \date   2012-10-08
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <vm_session_component.h>
#include <vm_root.h>

using Component = Core::Vm_session_component<Board::Vm_page_table>;

template <> Component::Attach_result Component::attach_pic(addr_t) {
	return Attach_error::INVALID_DATASPACE; }


Core::Vm_root::Create_result Core::Vm_root::_create_session(const char *args)
{
	Memory::Constrained_obj_allocator<Component> obj_alloc(*md_alloc());

	return obj_alloc.create(_registry, _vmid_alloc, *ep(),
		                    session_resources_from_args(args),
		                    session_label_from_args(args),
		                    _ram_allocator, _mapped_ram, _local_rm,
	                        _trace_sources).template convert<Create_result>(
		[&] (auto &a) -> Create_result {
			return a.obj.constructed().template convert<Create_result>(
				[&] (auto) -> Session_object<Vm_session> & {
					a.deallocate = false;
					return a.obj; },
				[&] (auto e) -> Create_error { return convert(e); });
		},
		[&] (auto e) -> Create_error{ return convert(e); });
}
