/*
 * \brief  Trace timestamp
 * \author Sebastian Sumpf
 * \date   2017-05-26
 *
 * Serialized reading of performance counter on ARM.
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SPEC__RISCV__TRACE__TIMESTAMP_H_
#define _INCLUDE__SPEC__RISCV__TRACE__TIMESTAMP_H_

#include <base/fixed_stdint.h>

namespace Genode { namespace Trace {

	typedef uint32_t Timestamp;

	inline Timestamp timestamp()
	{
		return 0;
	}
} }

#endif /* _INCLUDE__SPEC__RISCV__TRACE__TIMESTAMP_H_ */
