/*
 * \brief  Key into RPC object pool
 * \author Norman Feske
 * \date   2015-05-08
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__INTERNAL__RPC_OBJ_KEY_H_
#define _INCLUDE__BASE__INTERNAL__RPC_OBJ_KEY_H_

/* base includes */
#include <base/stdint.h>
#include <base/output.h>

namespace Genode { struct Rpc_obj_key; }


class Genode::Rpc_obj_key
{
	public:

		enum { INVALID = ~0UL };

	private:

		addr_t _value = INVALID;

	public:

		Rpc_obj_key() { }

		explicit Rpc_obj_key(addr_t value) : _value(value) { }

		bool   valid() const { return _value != INVALID; }
		addr_t value() const { return _value; }

		void print(Output &out) const
		{
			/*
			 * We print the value as signed long to make 'INVALID' or platform-
			 * specific low-level codes like 'Protocol_header::INVALID_BADGE'
			 * on Linux) easily recognizable. Such codes appear as negative
			 * numbers.
			 */
			Genode::print(out, "key=", (long)_value);
		}
};

#endif /* _INCLUDE__BASE__INTERNAL__RPC_OBJ_KEY_H_ */
