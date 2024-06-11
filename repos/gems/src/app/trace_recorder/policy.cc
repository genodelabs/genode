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


Trace_recorder::Policy::Id Trace_recorder::Policy::_init_policy()
{
	Id const id = _trace.alloc_policy(_size);

	id.with_result(
		[&] (Trace::Policy_id const policy_id) {
			Dataspace_capability dst_ds = _trace.policy(policy_id);
			if (!dst_ds.valid()) {
				warning("unable to obtain policy buffer");
				return;
			}

			Attached_dataspace const src { _env.rm(), _ds };
			Attached_dataspace       dst { _env.rm(), dst_ds };

			memcpy(dst.local_addr<char>(), src.local_addr<char const>(), _size.num_bytes);
		},
		[&] (Trace::Connection::Alloc_policy_error) {
			warning("failed to allocate policy buffer"); });

	return id;
}

