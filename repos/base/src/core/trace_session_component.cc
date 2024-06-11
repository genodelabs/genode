/*
 * \brief  TRACE session implementation
 * \author Norman Feske
 * \date   2013-08-12
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <dataspace/capability.h>
#include <base/rpc_client.h>

/* core-internal includes */
#include <trace/session_component.h>

using namespace Core;
using namespace Core::Trace;


Dataspace_capability Session_component::dataspace()
{
	return _argument_buffer.cap();
}


Session_component::Subjects_rpc_result Session_component::subjects()
{
	try {
		_subjects.import_new_sources(_sources);
	}
	catch (Out_of_ram)  { return Alloc_rpc_error::OUT_OF_RAM; }
	catch (Out_of_caps) { return Alloc_rpc_error::OUT_OF_CAPS; }

	return Num_subjects { _subjects.subjects(_argument_buffer.local_addr<Subject_id>(),
	                                         _argument_buffer.size()/sizeof(Subject_id)) };
}


Session_component::Infos_rpc_result Session_component::subject_infos()
{
	try {
		_subjects.import_new_sources(_sources);
	}
	catch (Out_of_ram)  { return Alloc_rpc_error::OUT_OF_RAM; }
	catch (Out_of_caps) { return Alloc_rpc_error::OUT_OF_CAPS; }

	unsigned const count = unsigned(_argument_buffer.size() /
	                                (sizeof(Subject_info) + sizeof(Subject_id)));

	Subject_info * const infos = _argument_buffer.local_addr<Subject_info>();
	Subject_id   * const ids   = reinterpret_cast<Subject_id *>(infos + count);

	return Num_subjects { _subjects.subjects(infos, ids, count) };
}


Session_component::Alloc_policy_rpc_result Session_component::alloc_policy(Policy_size size)
{
	size.num_bytes = min(size.num_bytes, _argument_buffer.size());

	Policy_id const id { ++_policy_cnt };

	return _ram.try_alloc(size.num_bytes).convert<Alloc_policy_rpc_result>(

		[&] (Ram_dataspace_capability const ds_cap) -> Alloc_policy_rpc_result {
			try {
				_policies.insert(*this, id, _policies_slab, ds_cap, size);
			}
			catch (Out_of_ram)  { _ram.free(ds_cap); return Alloc_policy_rpc_error::OUT_OF_RAM; }
			catch (Out_of_caps) { _ram.free(ds_cap); return Alloc_policy_rpc_error::OUT_OF_CAPS; }
			return id;
		},
		[&] (Ram_allocator::Alloc_error const e) -> Alloc_policy_rpc_result {
			switch (e) {
			case Ram_allocator::Alloc_error::OUT_OF_RAM:  return Alloc_policy_rpc_error::OUT_OF_RAM;
			case Ram_allocator::Alloc_error::OUT_OF_CAPS: return Alloc_policy_rpc_error::OUT_OF_CAPS;
			case Ram_allocator::Alloc_error::DENIED:      break;
			}
			return Alloc_policy_rpc_error::INVALID;
		});
}


Dataspace_capability Session_component::policy(Policy_id const id)
{
	Dataspace_capability result { };
	_policies.with_dataspace(*this, id, [&] (Dataspace_capability ds) {
		result = ds; });
	return result;
}


void Session_component::unload_policy(Policy_id const id)
{
	_policies.with_dataspace(*this, id, [&] (Dataspace_capability ds) {
		_policies.remove(*this, id);
		_ram.free(static_cap_cast<Ram_dataspace>(ds)); });
}


Session_component::Trace_rpc_result
Session_component::trace(Subject_id subject_id, Policy_id policy_id, Buffer_size size)
{
	Policy_size const policy_size = _policies.size(*this, policy_id);

	if (policy_size.num_bytes == 0)
		return Trace_rpc_error::INVALID_POLICY;

	Dataspace_capability const ds = policy(policy_id);

	auto rpc_result = [] (Subject::Trace_result const result) -> Trace_rpc_result
	{
		using Result = Subject::Trace_result;
		switch (result) {
		case Result::OK:              return Trace_ok { };
		case Result::OUT_OF_RAM:      return Trace_rpc_error::OUT_OF_RAM;
		case Result::OUT_OF_CAPS:     return Trace_rpc_error::OUT_OF_CAPS;
		case Result::FOREIGN:         return Trace_rpc_error::FOREIGN;
		case Result::SOURCE_IS_DEAD:  return Trace_rpc_error::SOURCE_IS_DEAD;
		case Result::INVALID_SUBJECT: break;
		};
		return Trace_rpc_error::INVALID_SUBJECT;
	};

	Trace_rpc_result result = Trace_rpc_error::INVALID_SUBJECT;

	_subjects.with_subject(subject_id, [&] (Subject &subject) {
		result = rpc_result(subject.trace(policy_id, ds, policy_size, _ram,
		                                  _local_rm, size)); });

	return result;
}


void Session_component::pause(Subject_id id)
{
	_subjects.with_subject(id, [&] (Subject &subject) { subject.pause(); });
}


void Session_component::resume(Subject_id id)
{
	_subjects.with_subject(id, [&] (Subject &subject) { subject.resume(); });
}


Dataspace_capability Session_component::buffer(Subject_id id)
{
	Dataspace_capability result { };
	_subjects.with_subject(id, [&] (Subject &subject) {
		result = subject.buffer(); });
	return result;
}


void Session_component::free(Subject_id id)
{
	_subjects.release(id);
}


Session_component::Session_component(Rpc_entrypoint  &ep,
                                     Resources const &resources,
                                     Label     const &label,
                                     Diag      const &diag,
                                     Ram_allocator   &ram,
                                     Region_map      &local_rm,
                                     size_t           arg_buffer_size,
                                     Source_registry &sources,
                                     Policy_registry &policies)
:
	Session_object(ep, resources, label, diag),
	_ram(ram, _ram_quota_guard(), _cap_quota_guard()),
	_local_rm(local_rm),
	_subjects_slab(&_md_alloc),
	_policies_slab(&_md_alloc),
	_sources(sources),
	_policies(policies),
	_subjects(_subjects_slab, _sources, _filter()),
	_argument_buffer(_ram, local_rm, arg_buffer_size)
{ }


Session_component::~Session_component()
{
	_policies.destroy_policies_owned_by(*this);
}
