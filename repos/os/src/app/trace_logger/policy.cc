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

/* Genode includes */
#include <base/attached_dataspace.h>

/* local includes */
#include <policy.h>

using namespace Genode;


Policy::Policy(Env &env, Trace::Connection &trace, Policy_dict &dict,
               Policy_name const &name)
:
	Policy_dict::Element(dict, name), _env(env), _trace(trace)
{
	id.with_result(
		[&] (Trace::Policy_id id) {
			Dataspace_capability const dst_ds = _trace.policy(id);
			if (dst_ds.valid()) {
				Attached_dataspace dst { _env.rm(), dst_ds },
				                   src { _env.rm(), _ds };
				memcpy(dst.local_addr<void>(), src.local_addr<void>(), _size);
				return;
			}
			warning("failed to obtain policy buffer for '", name, "'");
		},
		[&] (Trace::Connection::Alloc_policy_error) {
			warning("failed to allocate policy buffer for '", name, "'");
		});
}
