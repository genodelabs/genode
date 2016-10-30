/**
 * \brief  Rump initialization and required header
 * \author Sebastian Sumpf
 * \date   2014-01-17
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */
#ifndef _FILE_SYSTEM_H_
#define _FILE_SYSTEM_H_

/* Genode includes */
#include <util/xml_node.h>
#include <base/env.h>
#include <base/allocator.h>

extern "C" {
#include <sys/cdefs.h>
#include <sys/errno.h>
#include <fcntl.h>
#include <sys/dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/param.h>
#include <rump/rump.h>
#include <rump/rump_syscalls.h>
}

namespace File_system {
	void init(Genode::Env &, Genode::Allocator &heap, Genode::Xml_node config);
	bool supports_symlinks();
}

extern int errno;

#endif /* _FILE_SYSTEM_H_ */
