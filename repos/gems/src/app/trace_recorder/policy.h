/*
 * \brief  Installs and maintains a tracing policy
 * \author Johannes Schlatow
 * \date   2022-05-10
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _POLICY_H_
#define _POLICY_H_

/* Genode includes */
#include <util/dictionary.h>
#include <rom_session/connection.h>
#include <trace_session/connection.h>
#include <dataspace/client.h>

namespace Trace_recorder {
	class Policy;

	using Policy_name = Genode::String<64>;
	using Policies    = Genode::Dictionary<Policy, Policy_name>;
}


/**
 * Installs and maintains a tracing policy
 */
class Trace_recorder::Policy : Policies::Element
{
	public:

		using Id   = Genode::Trace::Connection::Alloc_policy_result;
		using Name = Policy_name;
		using Policies::Element::name;

	private:
		friend class Genode::Dictionary<Policy, Policy_name>;
		friend class Genode::Avl_node<Policy>;
		friend class Genode::Avl_tree<Policy>;

		Genode::Env                            &_env;
		Genode::Trace::Connection              &_trace;
		Genode::Rom_connection                  _rom;
		Genode::Rom_dataspace_capability const  _ds   { _rom.dataspace() };
		Genode::Trace::Policy_size       const  _size { Genode::Dataspace_client(_ds).size() };

		Id _init_policy();
		Id const _id = _init_policy();

	public:

		Policy(Genode::Env               &env,
		       Genode::Trace::Connection &trace,
		       Name                const &name,
		       Policies                  &policies)
		:
			Policies::Element(policies, name),
			_env(env), _trace(trace), _rom(env, name.string())
		{ }

		/***************
		 ** Accessors **
		 ***************/

		Id id() const { return _id; }
};

#endif /* _POLICY_H_ */
