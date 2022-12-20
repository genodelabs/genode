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

/* core-internal includes */
#include <trace/session_component.h>
#include <dataspace/capability.h>
#include <base/rpc_client.h>


using namespace Genode;
using namespace Genode::Trace;


Dataspace_capability Session_component::dataspace()
{
	return _argument_buffer.cap();
}


size_t Session_component::subjects()
{
	_subjects.import_new_sources(_sources);

	return _subjects.subjects(_argument_buffer.local_addr<Subject_id>(),
	                          _argument_buffer.size()/sizeof(Subject_id));
}


size_t Session_component::subject_infos()
{
	_subjects.import_new_sources(_sources);

	size_t const count  = _argument_buffer.size() / (sizeof(Subject_info) + sizeof(Subject_id));
	Subject_info *infos = _argument_buffer.local_addr<Subject_info>();
	Subject_id   *ids   = reinterpret_cast<Subject_id *>(infos + count);

	return _subjects.subjects(infos, ids, count);
}


Policy_id Session_component::alloc_policy(size_t size)
{
	if (size > _argument_buffer.size())
		throw Policy_too_large();

	/*
	 * Using prefix incrementation makes sure a policy with id == 0 is
	 * invalid.
	 */
	Policy_id const id(++_policy_cnt);

	Ram_dataspace_capability ds_cap = _ram.alloc(size); /* may throw */

	try {
		_policies.insert(*this, id, _policies_slab, ds_cap, size);
	} catch (...) {
		_ram.free(ds_cap);
		throw;
	}

	return id;
}


Dataspace_capability Session_component::policy(Policy_id id)
{
	return _policies.dataspace(*this, id);
}


void Session_component::unload_policy(Policy_id id)
{
	try {
		Dataspace_capability ds_cap = _policies.dataspace(*this, id);
		_policies.remove(*this, id);
		_ram.free(static_cap_cast<Ram_dataspace>(ds_cap));
	} catch (Nonexistent_policy) { }
}


void Session_component::trace(Subject_id subject_id, Policy_id policy_id,
                              size_t buffer_size)
{
	size_t const policy_size  = _policies.size(*this, policy_id);

	Trace::Subject &subject = _subjects.lookup_by_id(subject_id);

	subject.trace(policy_id, _policies.dataspace(*this, policy_id),
	              policy_size, _ram, _local_rm, buffer_size);
}


void Session_component::pause(Subject_id subject_id)
{
	_subjects.lookup_by_id(subject_id).pause();
}


void Session_component::resume(Subject_id subject_id)
{
	_subjects.lookup_by_id(subject_id).resume();
}


Dataspace_capability Session_component::buffer(Subject_id subject_id)
{
	return _subjects.lookup_by_id(subject_id).buffer();
}


void Session_component::free(Subject_id subject_id)
{
	_subjects.release(subject_id);
}


Session_component::Session_component(Rpc_entrypoint  &ep,
                                     Resources const &resources,
                                     Label     const &label,
                                     Diag      const &diag,
                                     Ram_allocator   &ram,
                                     Region_map      &local_rm,
                                     size_t           arg_buffer_size,
                                     unsigned         /* parent_levels */,
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
	_subjects(_subjects_slab, _sources),
	_argument_buffer(_ram, local_rm, arg_buffer_size)
{ }


Session_component::~Session_component()
{
	_policies.destroy_policies_owned_by(*this);
}
