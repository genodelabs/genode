/*
 * \brief  Core namespace declaration and basic types
 * \author Norman Feske
 * \date   2023-03-01
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__TYPES_H_
#define _CORE__INCLUDE__TYPES_H_

#include <util/noncopyable.h>
#include <util/reconstructible.h>
#include <util/interface.h>
#include <base/log.h>

namespace Core {

	using namespace Genode;

	struct Log2 { uint8_t log2; };

	enum class Access { READ, WRITE, EXEC };

	struct Addr
	{
		addr_t value;

		Addr reduced_by(addr_t offset) const
		{
			return { (value >= offset) ? (value - offset) : 0 };
		}

		Addr increased_by(addr_t offset) const
		{
			return { (value + offset >= offset) ? (value + offset) : 0 };
		}

		void print(Output &out) const { Genode::print(out, Hex(value)); }
	};

	struct Rwx
	{
		bool w, x;

		static constexpr bool r = true;

		static constexpr Rwx rwx() { return { true, true }; }

		void print(Output &out) const
		{
			Genode::print(out, "(r", w ? "w" : "-", x ? "x" : "-", ")");
		}
	};
}

namespace Genode {

	static inline void print(Output &out, Core::Access access)
	{
		switch (access) {
		case Core::Access::READ:  print(out, "READ");  break;
		case Core::Access::WRITE: print(out, "WRITE"); break;
		case Core::Access::EXEC:  print(out, "EXEC");  break;
		}
	}
}

#endif /* _CORE__INCLUDE__TYPES_H_ */
