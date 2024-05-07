/*
 * \brief  x86 kernel panic
 * \author Benjamin Lamowski
 * \date   2024-02-28
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__SPEC__X86_64__KERNEL__PANIC_H_
#define _CORE__SPEC__X86_64__KERNEL__PANIC_H_

/* base includes */
#include <base/log.h>

namespace Kernel {
	template <typename... ARGS>
	inline void panic(ARGS &&... args)
	{
		Genode::error("Kernel panic: ", args...);
		/* This is CPU local, but should be sufficient for now. */
		asm volatile(
			"cli;"
			"hlt;"
			: : :
		);
	}
}

#endif /* _CORE__SPEC__X86_64__KERNEL__PANIC_H_ */
