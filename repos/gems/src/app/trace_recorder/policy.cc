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

/* local includes */
#include <policy.h>

using namespace Genode;

Trace_recorder::Policy::Policy(Env                &env,
                               Trace::Connection  &trace,
                               Policy::Name const &name,
                               Policies           &registry)
:
	Policies::Element(registry, name),
	_env(env), _trace(trace), _rom(env, name.string())
{
		Dataspace_capability dst_ds = _trace.policy(_id);
		void *dst = _env.rm().attach(dst_ds);
		void *src = _env.rm().attach(_ds);
		memcpy(dst, src, _size);
		_env.rm().detach(dst);
		_env.rm().detach(src);
}
