/*
 * \brief  Registry containing tracing policy modules
 * \author Norman Feske
 * \date   2013-08-12
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__TRACE__POLICY_REGISTRY_H_
#define _CORE__INCLUDE__TRACE__POLICY_REGISTRY_H_

/* Genode includes */
#include <base/mutex.h>
#include <util/list.h>

/* core includes */
#include <types.h>

namespace Core { namespace Trace {
	class Policy_owner;
	class Policy;
	class Policy_registry;
} }


class Core::Trace::Policy_owner : Interface { };


struct Core::Trace::Policy : List<Policy>::Element
{
	Policy_owner const &_owner;
	Policy_id    const  _id;
	Allocator          &md_alloc;

	Ram::Allocator::Result const ds;

	Policy(Policy_owner const &owner, Policy_id id, Allocator &md_alloc,
	       Ram::Allocator &ram, Policy_size size)
	:
		_owner(owner), _id(id), md_alloc(md_alloc), ds(ram.try_alloc(size.num_bytes))
	{ }

	bool has_id  (Policy_id    const id)     const { return _id == id; }
	bool owned_by(Policy_owner const &owner) const { return &_owner == &owner; }

	Ram::Capability dataspace() const
	{
		return ds.convert<Ram::Capability>(
			[&] (Ram::Allocation const &a) { return a.cap; },
			[&] (Ram::Error)               { return Ram::Capability(); });
	}

	Policy_size size() const
	{
		return ds.convert<Policy_size>(
			[&] (Ram::Allocation const &a) -> Policy_size { return { a.num_bytes }; },
			[&] (Ram::Error)               -> Policy_size { return { }; });
	}
};


/**
 * Global policy registry
 */
class Core::Trace::Policy_registry
{

	private:

		Mutex        _mutex    { };
		List<Policy> _policies { };

		void _with_policy_unsynchronized(Policy_owner const &owner,
		                                 Policy_id const id, auto const &fn)
		{
			for (Policy *p = _policies.first(); p; p = p->next())
				if (p->owned_by(owner) && p->has_id(id))
					fn(*p);
		}

		Policy *_any_policy_owned_by(Policy_owner const &owner)
		{
			for (Policy *p = _policies.first(); p; p = p->next())
				if (p->owned_by(owner))
					return p;

			return nullptr;
		}

	public:

		~Policy_registry()
		{
			Mutex::Guard guard(_mutex);

			while (Policy *p = _policies.first())
				_policies.remove(p);
		}

		using Insert_result = Attempt<Ok, Ram::Error>;

		Insert_result insert(Policy_owner const &owner, Policy_id const id,
		                     Allocator &md_alloc, Ram::Allocator &ram,
		                     Policy_size size)
		{
			Mutex::Guard guard(_mutex);

			try {
				Policy &policy = *new (&md_alloc) Policy(owner, id, md_alloc, ram, size);
				_policies.insert(&policy);
				return policy.ds.convert<Insert_result>(
					[&] (Ram::Allocation const &) { return Ok(); },
					[&] (Ram::Error e)            { return e; });
			}
			catch (Out_of_ram)  { return Ram::Error::OUT_OF_RAM; }
			catch (Out_of_caps) { return Ram::Error::OUT_OF_CAPS; }
		}

		void remove(Policy_owner &owner, Policy_id id)
		{
			Mutex::Guard guard(_mutex);

			for (Policy *p = _policies.first(); p; ) {
				Policy *tmp = p;
				p = p->next();

				if (tmp->owned_by(owner) && tmp->has_id(id)) {
					_policies.remove(tmp);
					destroy(&tmp->md_alloc, tmp);
				}
			}
		}

		void destroy_policies_owned_by(Policy_owner const &owner)
		{
			Mutex::Guard guard(_mutex);

			while (Policy *p = _any_policy_owned_by(owner)) {
				_policies.remove(p);
				destroy(&p->md_alloc, p);
			}
		}

		void with_dataspace(Policy_owner &owner, Policy_id id, auto const &fn)
		{
			Mutex::Guard guard(_mutex);

			_with_policy_unsynchronized(owner, id, [&] (Policy &p) {
				fn(p.dataspace()); });
		}

		Policy_size size(Policy_owner &owner, Policy_id id)
		{
			Mutex::Guard guard(_mutex);

			Policy_size result { 0 };
			_with_policy_unsynchronized(owner, id, [&] (Policy const &p) {
				result = p.size(); });
			return result;
		}
};

#endif /* _CORE__INCLUDE__TRACE__POLICY_REGISTRY_H_ */
