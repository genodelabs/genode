/*
 * \brief  Allocator to manage CPU threads associated with a CPU session
 * \author Martin Stein
 * \date   2012-05-28
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _BASE__SRC__CORE__INCLUDE__CPU_THREAD_ALLOCATOR_H_
#define _BASE__SRC__CORE__INCLUDE__CPU_THREAD_ALLOCATOR_H_

/* Genode includes */
#include <base/tslab.h>

namespace Genode
{
	class Cpu_thread_component;

	/**
	 * Allocator to manage CPU threads associated with a CPU session
	 */
	typedef Tslab<Cpu_thread_component, 1024> Cpu_thread_allocator;
}

#endif /* _BASE__SRC__CORE__INCLUDE__CPU_THREAD_ALLOCATOR_H_ */

