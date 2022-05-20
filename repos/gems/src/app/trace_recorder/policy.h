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

/* local includes */
#include <named_registry.h>

/* Genode includes */
#include <rom_session/connection.h>
#include <trace_session/connection.h>
#include <dataspace/client.h>

namespace Trace_recorder {
	class Policy;

	using Policies    = Named_registry<Policy>;
}


/**
 * Installs and maintains a tracing policy
 */
class Trace_recorder::Policy : Policies::Element
{
	private:
		friend class Policies::Element;
		friend class Genode::Avl_node<Policy>;
		friend class Genode::Avl_tree<Policy>;

		Genode::Env                            &_env;
		Genode::Trace::Connection              &_trace;
		Genode::Rom_connection                  _rom;
		Genode::Rom_dataspace_capability const  _ds   { _rom.dataspace() };
		Genode::size_t                   const  _size { Genode::Dataspace_client(_ds).size() };
		Genode::Trace::Policy_id         const  _id   { _trace.alloc_policy(_size) };

	public:

		using Name = Policies::Element::Name;
		using Policies::Element::name;
		using Policies::Element::Element;

		Policy(Genode::Env               &env,
		       Genode::Trace::Connection &trace,
		       Name                const &name,
		       Policies                  &registry);


		/***************
		 ** Accessors **
		 ***************/

		Genode::Trace::Policy_id  id()   const { return _id; }
};

#endif /* _POLICY_H_ */
