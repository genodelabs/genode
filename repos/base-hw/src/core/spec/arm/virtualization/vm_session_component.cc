/*
 * \brief  VM session component for 'base-hw'
 * \author Stefan Kalkowski
 * \date   2015-02-17
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <vm_root.h>
#include <vm_session_component.h>

using Component = Core::Vm_session_component<Board::Vm_page_table>;

template <> Component::Attach_result Component::attach_pic(addr_t addr)
{
	return _attach(addr, Board::Cpu_mmio::IRQ_CONTROLLER_VT_CPU_BASE,
	               Board::Cpu_mmio::IRQ_CONTROLLER_VT_CPU_SIZE, false, true,
	               CACHED);
}


Core::Vm_root::Create_result Core::Vm_root::_create_session(const char *args)
{
	Memory::Constrained_obj_allocator<Component> obj_alloc(*md_alloc());

	return obj_alloc.create(_registry, _vmid_alloc, *ep(),
		                    session_resources_from_args(args),
		                    session_label_from_args(args),
		                    _ram_allocator, _mapped_ram, _local_rm,
	                        _trace_sources).template convert<Create_result>(
		[&] (auto &a) -> Create_result {
			return a.obj.constructed.template convert<Create_result>(
				[&] (auto) -> Session_object<Vm_session> & {
					a.deallocate = false;
					return a.obj; },
				[&] (auto err) -> Create_error {
					switch(err) {
					case decltype(err)::OUT_OF_RAM:
						return Create_error::INSUFFICIENT_RAM;
					case decltype(err)::DENIED: ;
					}
					return Create_error::DENIED;
				});
		},
		[&] (auto err) -> Create_error{
			switch(err) {
			case Alloc_error::OUT_OF_RAM:  return Create_error::INSUFFICIENT_RAM;
			case Alloc_error::OUT_OF_CAPS: return Create_error::INSUFFICIENT_CAPS;
			case Alloc_error::DENIED:      break;
			}
			return Create_error::DENIED;
		});
}
