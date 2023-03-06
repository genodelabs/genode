/*
 * \brief  Genode resource path
 * \author Norman Feske
 * \date   2012-08-10
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__RESOURCE_PATH_H_
#define _CORE__INCLUDE__RESOURCE_PATH_H_

/* Linux syscall bindings */
#include <linux_syscalls.h>

/* Genode includes */
#include <util/string.h>

/**
 * Return resource path for Genode
 *
 * Genode creates files for dataspaces and endpoints under in this directory.
 */
static inline char const *resource_path()
{
	static Genode::String<32> path("/tmp/genode-", lx_getuid());
	return path.string();
}

#endif /* _CORE__INCLUDE__RESOURCE_PATH_H_ */
