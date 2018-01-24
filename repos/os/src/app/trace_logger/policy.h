/*
 * \brief  Installs and maintains a tracing policy
 * \author Martin Stein
 * \date   2018-01-12
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _POLICY_H_
#define _POLICY_H_

/* Genode includes */
#include <rom_session/connection.h>
#include <trace_session/connection.h>
#include <dataspace/client.h>
#include <util/avl_string.h>

using Policy_name = Genode::String<40>;

class Policy;


/**
 * Member of a policy that allows the policy to be managed in a tree
 */
class Policy_avl_member : public Genode::Avl_string_base
{
	private:

		::Policy &_policy;

	public:

		Policy_avl_member(Policy_name const &name,
		                  ::Policy          &policy);


		/***************
		 ** Accessors **
		 ***************/

		::Policy &policy() const { return _policy; }
};


/**
 * Ensure that policy name is constructed before it is used as tree index
 */
class Policy_base
{
	protected:

		Policy_name const _name;

		Policy_base(Policy_name const &name) : _name(name) { }
};


/**
 * Installs and maintains a tracing policy
 */
class Policy : public Policy_base
{
	private:

		Policy_avl_member                       _avl_member;
		Genode::Env                            &_env;
		Genode::Trace::Connection              &_trace;
		Genode::Rom_connection                  _rom  { _env, _name.string() };
		Genode::Rom_dataspace_capability const  _ds   { _rom.dataspace() };
		Genode::size_t                   const  _size { Genode::Dataspace_client(_ds).size() };
		Genode::Trace::Policy_id         const  _id   { _trace.alloc_policy(_size) };

	public:


		Policy(Genode::Env               &env,
		       Genode::Trace::Connection &trace,
		       Policy_name         const &name);


		/***************
		 ** Accessors **
		 ***************/

		Genode::Trace::Policy_id  id()   const { return _id; }
		Policy_avl_member        &avl_member() { return _avl_member; }
};


/**
 * AVL tree of policies with their name as index
 */
struct Policy_tree : Genode::Avl_tree<Genode::Avl_string_base>
{
	using Avl_tree = Genode::Avl_tree<Genode::Avl_string_base>;

	struct No_match : Genode::Exception { };

	static ::Policy &policy(Genode::Avl_string_base const &node);

	::Policy &find_by_name(Policy_name name);

	template <typename FUNC>
	void for_each(FUNC && functor) const {
		Avl_tree::for_each([&] (Genode::Avl_string_base const &node) {
			functor(policy(node));
		});
	}

	void insert(::Policy &policy) { Avl_tree::insert(&policy.avl_member()); }
};


#endif /* _POLICY_H_ */
