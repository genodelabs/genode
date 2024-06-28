/*
 * \brief  Allocator to manage CPU threads associated with a CPU session
 * \author Martin Stein
 * \date   2012-05-28
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__CPU_THREAD_ALLOCATOR_H_
#define _CORE__INCLUDE__CPU_THREAD_ALLOCATOR_H_

/* Genode includes */
#include <base/tslab.h>
#include <base/heap.h>

/* core includes */
#include <types.h>

/* base-internal includes */
#include <base/internal/page_size.h>

namespace Core {

	class Cpu_thread_component;

	/**
	 * Allocator to manage CPU threads associated with a CPU session
	 *
	 * We take the knowledge about the used backing-store allocator (sliced
	 * heap) into account to make sure that slab blocks fill whole pages.
	 */
	using Cpu_thread_allocator =
		Tslab<Cpu_thread_component, get_page_size() - Sliced_heap::meta_data_size(), 2>;
}

#endif /* _CORE__INCLUDE__CPU_THREAD_ALLOCATOR_H_ */
