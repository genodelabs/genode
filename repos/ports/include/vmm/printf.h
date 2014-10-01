/*
 * \brief  Utilities for implementing VMMs on Genode/NOVA
 * \author Norman Feske
 * \date   2013-08-20
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__VMM__PRINTF_H_
#define _INCLUDE__VMM__PRINTF_H_

/* Genode includes */
#include <base/thread.h>
#include <base/lock.h>
#include <base/printf.h>

namespace Vmm {

	using namespace Genode;

	void printf(const char *format, ...) __attribute__((format(printf, 1, 2)));
}


/**
 * Print message while preserving the UTCB content
 */
inline void Vmm::printf(const char *format, ...)
{
	va_list list;
	va_start(list, format);

	static Lock        lock;
	static Native_utcb utcb_backup;

	Lock::Guard guard(lock);

	utcb_backup = *Thread_base::myself()->utcb();

	Genode::printf("VMM: ");
	Genode::vprintf(format, list);

	*Thread_base::myself()->utcb() = utcb_backup;

	va_end(list);
}

#endif /* _INCLUDE__VMM__PRINTF_H_ */
