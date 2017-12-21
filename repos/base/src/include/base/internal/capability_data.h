/*
 * \brief  Capability meta data that is common for core and non-core components
 * \author Norman Feske
 * \date   2015-05-06
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__INTERNAL__CAPABILITY_DATA_H_
#define _INCLUDE__BASE__INTERNAL__CAPABILITY_DATA_H_

/* base includes */
#include <util/noncopyable.h>
#include <util/avl_tree.h>

/* base-internal includes */
#include <base/internal/rpc_obj_key.h>

namespace Genode { class Capability_data; }


class Genode::Capability_data
{
	private:

		uint8_t     _ref_cnt = 0;      /* reference counter */
		Rpc_obj_key _rpc_obj_key { };  /* key into RPC object pool */

	public:

		Capability_data(Rpc_obj_key rpc_obj_key)
		: _rpc_obj_key(rpc_obj_key) { }

		Capability_data() { }

		Rpc_obj_key rpc_obj_key() const { return _rpc_obj_key; }

		uint8_t inc_ref() { return ++_ref_cnt; }
		uint8_t dec_ref() { return --_ref_cnt; }
};

#endif /* _INCLUDE__BASE__INTERNAL__CAPABILITY_DATA_H_ */
