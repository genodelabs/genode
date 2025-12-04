/*
 * \brief  VM session component for 'base-hw'
 * \author Stefan Kalkowski
 * \date   2012-10-08
 */

/*
 * Copyright (C) 2012-2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <spec/x86_64/ept.h>
#include <spec/x86_64/hpt.h>
#include <vm_root.h>
#include <vm_session_component.h>

using namespace Genode;
using namespace Core;

template <>
Vm_session::Attach_result Vm_session_component<Hw::Ept>::attach_pic(addr_t)
{
	return Attach_error::INVALID_DATASPACE;
}


template <>
Vm_session::Attach_result Vm_session_component<Hw::Hpt>::attach_pic(addr_t)
{
	return Attach_error::INVALID_DATASPACE;
}


/*******************
 ** Core::Vm_root **
 *******************/

using Result = Core::Vm_root::Create_result;

template <typename T>
static Result _create(Memory::Constrained_allocator  &alloc,
                      Registry<Revoke>               &registry,
                      Vmid_allocator                 &id_alloc,
                      Rpc_entrypoint                 &ep,
                      const char                     *args,
                      Ram_allocator                  &ram,
                      Mapped_ram_allocator           &mapped_ram,
                      Local_rm                       &rm,
                      Core::Trace::Source_registry   &trace_sources)
{
	using Error = Core::Vm_root::Create_error;

	Memory::Constrained_obj_allocator<T> obj_alloc(alloc);

	return obj_alloc.create(registry, id_alloc, ep,
	                        session_resources_from_args(args),
	                        session_label_from_args(args),
	                        ram, mapped_ram, rm,
	                        trace_sources).template convert<Result>(
		[&] (auto &a) -> Result {
			return a.obj.constructed.template convert<Result>(
				[&] (auto) -> T & {
					a.deallocate = false;
					return a.obj; },
				[&] (auto error) -> Error {
					using E = Core::Accounted_mapped_ram_allocator::Error;
					switch(error) {
					case E::OUT_OF_RAM: return Error::INSUFFICIENT_RAM;
					case E::DENIED:     break;
					}
					return Error::DENIED;
				});
		},
		[&] (auto error) -> Error {
			switch(error) {
			case Alloc_error::OUT_OF_RAM:  return Error::INSUFFICIENT_RAM;
			case Alloc_error::OUT_OF_CAPS: return Error::INSUFFICIENT_CAPS;
			case Alloc_error::DENIED:      break;
			}
			return Error::DENIED;
		});
}


Core::Vm_root::Create_result Vm_root::_create_session(const char *args)
{
	using Vmx_session_component = Vm_session_component<Hw::Ept>;
	using Svm_session_component = Vm_session_component<Hw::Hpt>;

	if (Hw::Virtualization_support::has_svm())
		return ::_create<Svm_session_component>(*md_alloc(), _registry,
		                                        _vmid_alloc, *ep(), args,
		                                        _ram_allocator, _mapped_ram,
		                                        _local_rm, _trace_sources);

	if (Hw::Virtualization_support::has_vmx())
		return ::_create<Vmx_session_component>(*md_alloc(), _registry,
		                                        _vmid_alloc, *ep(), args,
		                                        _ram_allocator, _mapped_ram,
		                                        _local_rm, _trace_sources);

	return Create_error::DENIED;
}
