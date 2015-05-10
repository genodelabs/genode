/*
 * \brief  Key into RPC object pool
 * \author Norman Feske
 * \date   2015-05-08
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _BASE__INTERNAL__RPC_OBJ_KEY_H_
#define _BASE__INTERNAL__RPC_OBJ_KEY_H_

/* base includes */
#include <base/stdint.h>

namespace Genode { struct Rpc_obj_key; }


class Genode::Rpc_obj_key
{
	public:

		enum { INVALID = ~0UL };

	private:

		uint32_t _value = INVALID;

	public:

		Rpc_obj_key() { }

		explicit Rpc_obj_key(uint32_t value) : _value(value) { }

		bool     valid() const { return _value != INVALID; }
		uint32_t value() const { return _value; }
};

#endif /* _BASE__INTERNAL__RPC_OBJ_KEY_H_ */
