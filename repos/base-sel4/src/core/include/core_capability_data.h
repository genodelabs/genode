/*
 * \brief  Definition of core-specific capability meta data
 * \author Norman Feske
 * \date   2015-05-08
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__CORE_CAPABILITY_DATA_H_
#define _CORE__INCLUDE__CORE_CAPABILITY_DATA_H_

/* base-internal includes */
#include <base/internal/capability_data.h>

/* core includes */
#include <types.h>

namespace Core {

	class Cap_session;
	class Core_capability_data;
}


class Core::Core_capability_data : public Capability_data
{
	private:

		Cap_session const *_cap_session;

	public:

		Capability_core_data(Cap_session const *cap_session, Rpc_obj_key key)
		:
			Capability_common_data(key), _cap_session(cap_session)
		{ }

		bool belongs_to(Cap_session const *session) const
		{
			return _cap_session == session;
		}
};

#endif /* _CORE__INCLUDE__CORE_CAPABILITY_DATA_H_ */
