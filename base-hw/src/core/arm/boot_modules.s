/*
 * \brief  Dummy boot-modules-file to enable a 'core' standalone image
 * \author Martin Stein
 * \date   2011-12-16
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

.section .data

.align 3
.global _boot_modules_begin
_boot_modules_begin:
.string "GROM"

.align 3
.global _boot_module_headers_begin
_boot_module_headers_begin:

/* no module headers */

.global _boot_module_headers_end
_boot_module_headers_end:

/* no modules */

.global _boot_modules_end
_boot_modules_end:

