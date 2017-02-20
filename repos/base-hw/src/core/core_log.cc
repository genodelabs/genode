/*
 * \brief  Access to the core's log facility
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \date   2016-05-03
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* base-internal includes */
#include <base/internal/output.h>
#include <base/internal/raw_write_string.h>

#include <core_log.h>
#include <kernel/log.h>


void Genode::Core_log::out(char const c) { Kernel::log(c); }


void Genode::raw_write_string(char const *str) {
	while (char c = *str++) Kernel::log(c); }
