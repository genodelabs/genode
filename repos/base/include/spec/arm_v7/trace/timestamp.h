/*
 * \brief  Trace timestamp
 * \author Stefan Kalkowski
 * \date   2013-08-20
 *
 * Serialized reading of performance counter on ARM.
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SPEC__ARM_V7__TRACE__TIMESTAMP_H_
#define _INCLUDE__SPEC__ARM_V7__TRACE__TIMESTAMP_H_

#include <base/fixed_stdint.h>

namespace Genode { namespace Trace {

	typedef uint32_t Timestamp;

	inline Timestamp timestamp()
	{
		uint32_t t;
		asm volatile("mrc p15, 0, %0, c9, c13, 0" : "=r"(t));
		return t;
	}
} }

#endif /* _INCLUDE__SPEC__ARM_V7__TRACE__TIMESTAMP_H_ */
