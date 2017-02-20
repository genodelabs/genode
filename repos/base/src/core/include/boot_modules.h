/*
 * \brief  Boot modules
 * \author Stefan Kalkowski
 * \date   2016-09-15
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__BOOT_MODULES_H_
#define _CORE__INCLUDE__BOOT_MODULES_H_

namespace Genode { struct Boot_modules_header; }

struct Genode::Boot_modules_header
{
	long name; /* physical address of null-terminated string */
	long base; /* physical address of module data */
	long size; /* size of module data in bytes */
};

extern Genode::Boot_modules_header _boot_modules_headers_begin;
extern Genode::Boot_modules_header _boot_modules_headers_end;
extern int                         _boot_modules_binaries_begin;
extern int                         _boot_modules_binaries_end;

#endif /* _CORE__INCLUDE__BOOT_MODULES_H_ */
