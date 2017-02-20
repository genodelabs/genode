/*
 * \brief  Interfaces for initializing libc subsystems
 * \author Norman Feske
 * \date   2016-10-27
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LIBC_INIT_H_
#define _LIBC_INIT_H_

/* Genode includes */
#include <base/env.h>
#include <util/xml_node.h>

namespace Libc {

	/**
	 * Support for shared libraries
	 */
	void init_dl(Genode::Env &env);

	/**
	 * Global memory allocator
	 */
	void init_mem_alloc(Genode::Env &env);

	/**
	 * Support for querying available RAM quota in sysctl functions
	 */
	void sysctl_init(Genode::Env &env);

	/**
	 * Set libc config node
	 */
	void libc_config_init(Genode::Xml_node node);
}

#endif /* _LIBC_INIT_H_ */
