/*
 * \brief  Basic Genode types
 * \author Martin Stein
 * \author Stefan Kalkowski
 * \date   2012-01-02
 */

/*
 * Copyright (C) 2012-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__NATIVE_TYPES_H_
#define _INCLUDE__BASE__NATIVE_TYPES_H_

/* Genode includes */
#include <kernel/log.h>
#include <base/native_capability.h>
#include <base/ipc_msgbuf.h>
#include <base/printf.h>

namespace Genode
{
	class Platform_thread;
	class Native_thread;

	typedef int Native_connection_state;

	/**
	 * Coherent address region
	 */
	struct Native_region;
}


struct Genode::Native_thread
{
	Platform_thread  * platform_thread;
	Native_capability  cap;
};



#endif /* _INCLUDE__BASE__NATIVE_TYPES_H_ */
