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
	return _argument_ds.convert<Dataspace_capability>(
		[&] (Ram_allocator::Allocation &a) { return a.cap; },
		[&] (Alloc_error) { return Dataspace_capability(); });
}


Session_component::Subjects_rpc_result Session_component::subjects()
{
	return _subjects.import_new_sources(_sources).convert<Subjects_rpc_result>(
		[&] (Ok) -> Subjects_rpc_result {
			return _argument_mapped.convert<Subjects_rpc_result>(
				[&] (Local_rm::Attachment &local) -> Num_subjects {
					return { _subjects.subjects((Subject_id *)local.ptr,
					                            local.num_bytes/sizeof(Subject_id)) };
				},
				[&] (Local_rm::Error) { return Alloc_error::DENIED; });
		},
		[&] (Alloc_error e) { return e; });
}


Session_component::Infos_rpc_result Session_component::subject_infos()
{
	return _subjects.import_new_sources(_sources).convert<Subjects_rpc_result>(
		[&] (Ok) -> Infos_rpc_result {

			return _argument_mapped.convert<Subjects_rpc_result>(
				[&] (Local_rm::Attachment &local) -> Num_subjects {

					unsigned const count = unsigned(local.num_bytes /
					                                (sizeof(Subject_info) + sizeof(Subject_id)));

					Subject_info * const infos = (Subject_info *)local.ptr;
					Subject_id   * const ids   = reinterpret_cast<Subject_id *>(infos + count);

					return Num_subjects { _subjects.subjects(infos, ids, count) };
				},
				[&] (Local_rm::Error) { return Alloc_error::DENIED; });
		},
		[&] (Alloc_error e) { return e; });
}


Session_component::Alloc_policy_rpc_result Session_component::alloc_policy(Policy_size size)
{
	size_t const argument_buffer_size = _argument_mapped.convert<size_t>(
		[&] (Local_rm::Attachment const &a) { return a.num_bytes; },
		[&] (Local_rm::Error)               { return 0ul; });

	size.num_bytes = min(size.num_bytes, argument_buffer_size);

	Policy_id const id { _policy_cnt + 1 };

	return _policies.insert(*this, id, _policy_alloc, _ram, size)
		.convert<Alloc_policy_rpc_result>(
			[&] (Ok) { _policy_cnt++; return id; },
			[&] (Alloc_error e) { return e; });
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
		/* deallocate via '~Allocation' */
		Ram::Allocation { _ram, { static_cap_cast<Ram_dataspace>(ds), 0 } };
	});
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
		case Result::OK:              return Ok { };
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
                                     Local_rm        &local_rm,
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
	_argument_ds(_ram.try_alloc(arg_buffer_size)),
	_argument_mapped(_argument_ds.convert<Local_rm::Result>(
		[&] (Ram_allocator::Allocation const &ram) {
			return local_rm.attach(ram.cap, {
				.size       = { }, .offset    = { },
				.use_at     = { }, .at        = { },
				.executable = { }, .writeable = true });
		},
		[&] (Alloc_error) { return Local_rm::Error::INVALID_DATASPACE; }
	))
{ }


Session_component::~Session_component()
{
	_policies.destroy_policies_owned_by(*this);
}
