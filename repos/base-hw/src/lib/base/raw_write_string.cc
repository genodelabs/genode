/*
 * \brief  Kernel-specific raw-output back end
 * \author Norman Feske
 * \date   2016-03-08
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <kernel/interface.h>
#include <base/internal/raw_write_string.h>

void Genode::raw_write_string(char const *str) {
	while (char c = *str++) Kernel::print_char(c); }
