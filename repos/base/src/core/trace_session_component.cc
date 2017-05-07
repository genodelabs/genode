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
	return _argument_buffer.ds;
}


size_t Session_component::subjects()
{
	_subjects.import_new_sources(_sources);

	return _subjects.subjects((Subject_id *)_argument_buffer.base,
	                          _argument_buffer.size/sizeof(Subject_id));
}


Policy_id Session_component::alloc_policy(size_t size)
{
	if (size > _argument_buffer.size)
		throw Policy_too_large();

	/*
	 * Using prefix incrementation makes sure a policy with id == 0 is
	 * invalid.
	 */
	Policy_id const id(++_policy_cnt);

	if (!_md_alloc.withdraw(size))
		throw Out_of_ram();

	try {
		Ram_dataspace_capability ds = _ram.alloc(size);

		_policies.insert(*this, id, _policies_slab, ds, size);
	} catch (...) {
		/* revert withdrawal or quota */
		_md_alloc.upgrade(size);
		throw Out_of_ram();
	}

	return id;
}


Dataspace_capability Session_component::policy(Policy_id id)
{
	return _policies.dataspace(*this, id);
}


void Session_component::unload_policy(Policy_id id)
{
	_policies.remove(*this, id);
}


void Session_component::trace(Subject_id subject_id, Policy_id policy_id,
                              size_t buffer_size)
{
	size_t const  policy_size = _policies.size(*this, policy_id);
	size_t const required_ram = buffer_size + policy_size;

	/*
	 * Account RAM needed for trace buffer and policy buffer to the trace
	 * session.
	 */
	if (!_md_alloc.withdraw(required_ram))
		throw Out_of_ram();

	try {
		Trace::Subject *subject = _subjects.lookup_by_id(subject_id);
		subject->trace(policy_id, _policies.dataspace(*this, policy_id),
		               policy_size, _ram, buffer_size);
	} catch (...) {
		/* revert withdrawal or quota */
		_md_alloc.upgrade(required_ram);
		throw Out_of_ram();
	}
}


void Session_component::rule(Session_label const &, Thread_name const &,
                             Policy_id, size_t)
{
	/* not implemented yet */
}


void Session_component::pause(Subject_id subject_id)
{
	_subjects.lookup_by_id(subject_id)->pause();
}


void Session_component::resume(Subject_id subject_id)
{
	_subjects.lookup_by_id(subject_id)->resume();
}


Subject_info Session_component::subject_info(Subject_id subject_id)
{
	return _subjects.lookup_by_id(subject_id)->info();
}


Dataspace_capability Session_component::buffer(Subject_id subject_id)
{
	return _subjects.lookup_by_id(subject_id)->buffer();
}


void Session_component::free(Subject_id subject_id)
{
	size_t released_ram = _subjects.lookup_by_id(subject_id)->release();
	_md_alloc.upgrade(released_ram);
}


Session_component::Session_component(Allocator &md_alloc, size_t ram_quota,
                                     size_t arg_buffer_size, unsigned parent_levels,
                                     char const *label, Source_registry &sources,
                                     Policy_registry &policies)
:
	_ram(*env_deprecated()->ram_session()),
	_md_alloc(&md_alloc, ram_quota),
	_subjects_slab(&_md_alloc),
	_policies_slab(&_md_alloc),
	_parent_levels(parent_levels),
	_label(label),
	_sources(sources),
	_policies(policies),
	_subjects(_subjects_slab, _ram, _sources),
	_argument_buffer(_ram, arg_buffer_size)
{
	_md_alloc.withdraw(_argument_buffer.size);
}


Session_component::~Session_component()
{
	_policies.destroy_policies_owned_by(*this);
}
