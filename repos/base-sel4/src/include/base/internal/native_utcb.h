/*
 * \brief  UTCB definition
 * \author Norman Feske
 * \date   2016-03-08
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__INTERNAL__NATIVE_UTCB_H_
#define _INCLUDE__BASE__INTERNAL__NATIVE_UTCB_H_

#include <base/stdint.h>

namespace Genode { struct Native_utcb; }

struct Genode::Native_utcb
{
	/**
	 * On seL4, the UTCB is called IPC buffer. We use one page
	 * for each IPC buffer.
	 */
	enum { IPC_BUFFER_SIZE = 4096, ELEMENTS = IPC_BUFFER_SIZE/sizeof(addr_t) };

	addr_t _raw[ELEMENTS];

	Native_utcb() { };

	addr_t ep_sel()   const { return _raw[ELEMENTS - 1]; }
	addr_t lock_sel() const { return _raw[ELEMENTS - 2]; }

	void ep_sel  (addr_t sel) { _raw[ELEMENTS - 1] = sel; }
	void lock_sel(addr_t sel) { _raw[ELEMENTS - 2] = sel; }
};

#endif /* _INCLUDE__BASE__INTERNAL__NATIVE_UTCB_H_ */
