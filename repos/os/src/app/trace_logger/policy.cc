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


Policy::Policy(Env &env, Trace::Connection &trace, Policy_dict &dict,
               Policy_name const &name)
:
	Policy_dict::Element(dict, name), _env(env), _trace(trace)
{
	id.with_result(
		[&] (Trace::Policy_id id) {
			Dataspace_capability const dst_ds = _trace.policy(id);
			if (dst_ds.valid()) {
				void       * const dst = _env.rm().attach(dst_ds);
				void const * const src = _env.rm().attach(_ds);
				memcpy(dst, src, _size);
				_env.rm().detach(dst);
				_env.rm().detach(src);
				return;
			}
			warning("failed to obtain policy buffer for '", name, "'");
		},
		[&] (Trace::Connection::Alloc_policy_error) {
			warning("failed to allocate policy buffer for '", name, "'");
		});
}
