/**
 * \brief  Trace support for linker intialization
 * \author Sebastian Sumpf
 * \date   2014-10-26
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__TRACE_H_
#define _INCLUDE__TRACE_H_

#if 0
typedef Genode::addr_t l4_umword_t;
typedef Genode::addr_t l4_addr_t;

namespace Fiasco {
	#include <l4/sys/ktrace.h>
}
#else
namespace Fiasco {
	inline void fiasco_tbuf_log_3val(char const *,unsigned, unsigned, unsigned) { }
}
extern "C" void wait_for_continue();
#endif

namespace Linker {
	inline void trace(char const *str, unsigned v1, unsigned v2, unsigned v3)
	{
		Fiasco::fiasco_tbuf_log_3val(str, v1, v2, v3);
	}
}

#endif /* _INCLUDE__TRACE_H_ */
