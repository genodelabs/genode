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
#include <util/dictionary.h>

class Policy;

using Policy_name = Genode::String<40>;
using Policy_dict = Genode::Dictionary<Policy, Policy_name>;


/**
 * Installs and maintains a tracing policy
 */
class Policy : public Policy_dict::Element
{
	private:

		Genode::Env                            &_env;
		Genode::Trace::Connection              &_trace;
		Genode::Rom_connection                  _rom  { _env, name };
		Genode::Rom_dataspace_capability const  _ds   { _rom.dataspace() };
		Genode::size_t                   const  _size { Genode::Dataspace_client(_ds).size() };

	public:

		using Id = Genode::Trace::Connection::Alloc_policy_result;

		Id const id { _trace.alloc_policy({_size}) };

		Policy(Genode::Env               &env,
		       Genode::Trace::Connection &trace,
		       Policy_dict               &dict,
		       Policy_name         const &name);
};

#endif /* _POLICY_H_ */
