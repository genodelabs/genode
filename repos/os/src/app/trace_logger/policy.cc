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

/* local includes */
#include <policy.h>

using namespace Genode;


/***********************
 ** Policy_avl_member **
 ***********************/

Policy_avl_member::Policy_avl_member(Policy_name const &name,
                                     ::Policy          &policy)
:
	Avl_string_base(name.string()), _policy(policy)
{ }


/************
 ** Policy **
 ************/

Policy::Policy(Env &env, Trace::Connection &trace, Policy_name const &name)
:
	Policy_base(name), _avl_member(_name, *this), _env(env), _trace(trace)
{
		Dataspace_capability dst_ds = _trace.policy(_id);
		void *dst = _env.rm().attach(dst_ds);
		void *src = _env.rm().attach(_ds);
		memcpy(dst, src, _size);
		_env.rm().detach(dst);
		_env.rm().detach(src);
}


/*****************
 ** Policy_tree **
 *****************/

Policy &Policy_tree::policy(Avl_string_base const &node)
{
	return static_cast<Policy_avl_member const *>(&node)->policy();
}


Policy &Policy_tree::find_by_name(Policy_name name)
{
	if (name == Policy_name() || !first()) {
		throw No_match(); }

	Avl_string_base *node = first()->find_by_name(name.string());
	if (!node) {
		throw No_match(); }

	return policy(*node);
}
