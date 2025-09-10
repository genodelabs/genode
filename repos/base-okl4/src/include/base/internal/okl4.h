/*
 * \brief  OKL4 system-call bindings
 * \author Norman Feske
 * \date   2017-12-22
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _BASE__INTERNAL__OKL4_H_
#define _BASE__INTERNAL__OKL4_H_

/* OKL4 includes */
namespace Okl4 { extern "C" {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wconversion"

#include <l4/config.h>
#include <l4/types.h>
#include <l4/ipc.h>
#include <l4/utcb.h>
#include <l4/thread.h>
#include <l4/kdebug.h>
#include <l4/schedule.h>
#include <l4/space.h>
#include <l4/map.h>
#include <l4/interrupt.h>
#include <l4/security.h>

#pragma GCC diagnostic pop
#undef UTCB_SIZE
} }

#include <base/internal/native_utcb.h>

namespace Okl4 {

	/*
	 * Read global thread ID from user-defined handle and store it
	 * into a designated UTCB entry.
	 */
	static inline L4_Word_t copy_uregister_to_utcb()
	{
		using namespace Okl4;

		L4_Word_t my_global_id = L4_UserDefinedHandle();
		__L4_TCR_Set_ThreadWord(Genode::UTCB_TCR_THREAD_WORD_MYSELF,
		                        my_global_id);
		return my_global_id;
	}
}

#endif /* _BASE__INTERNAL__OKL4_H_ */
