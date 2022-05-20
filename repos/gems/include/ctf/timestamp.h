/*
 * \brief  Generic timestamp for CTF traces
 * \author Johannes Schlatow
 * \date   2021-08-04
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CTF__TIMESTAMP_H_
#define _CTF__TIMESTAMP_H_

#include <base/fixed_stdint.h>
#include <util/register.h>

namespace Ctf {
	using namespace Genode;

	/* generic fixed-width timestamp type for different Trace::Timestamp widths */
	template <int PWIDTH>
	struct _Timestamp : Register<64> {
		static constexpr bool extended() { return PWIDTH < 64; }

		/* define _PWIDTH to get a valid Extension Bitfield even for 64-bit */
		enum { _PWIDTH = PWIDTH < 64 ? PWIDTH : 0 };

		struct Base      : Bitfield<0, PWIDTH>           { };
		struct Extension : Bitfield<_PWIDTH, 64-_PWIDTH> { };
	};

	typedef _Timestamp<sizeof(Trace::Timestamp)*8> Timestamp;

	typedef uint64_t __attribute__((aligned(1))) Timestamp_base;
}

#endif /* _CTF__TIMESTAMP_H_ */
