/*
 * \brief  Dummy boot-modules-file for building standalone images of core
 * \author Martin Stein
 * \date   2011-12-16
 */

/*
 * Copyright (C) 2011-2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

.section .data

.global _boot_modules_headers_begin
_boot_modules_headers_begin:

/* no headers */

.global _boot_modules_headers_end
_boot_modules_headers_end:

.section .data.boot_modules_binaries

.global _boot_modules_binaries_begin
_boot_modules_binaries_begin:

/* no binaries */

.global _boot_modules_binaries_end
_boot_modules_binaries_end:
