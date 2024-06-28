/*
 * \brief  Trace timestamp
 * \author Norman Feske
 * \date   2021-05-01
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SPEC__ARM_64__TRACE__TIMESTAMP_H_
#define _INCLUDE__SPEC__ARM_64__TRACE__TIMESTAMP_H_

#include <base/fixed_stdint.h>

namespace Genode { namespace Trace {

	using Timestamp = uint64_t;

	/*
	 * In Linux/AARCH64, the 'mrs' instruction cannot be executed in user land.
	 * It triggers the abort of the program with an illegal-instruction
	 * exception.
	 *
	 * By returning 0, we discharge the timestamp-based interpolation of
	 * 'Timer::Connection::curr_time', falling back to a precision of 1 ms.
	 */
	inline Timestamp timestamp() { return 0; }
} }

#endif /* _INCLUDE__SPEC__ARM_64__TRACE__TIMESTAMP_H_ */
