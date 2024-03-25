/*
 * \brief  Linux Kernel log messages
 * \author Stefan Kalkowski
 * \author Christian Helmuth
 * \date   2021-03-22
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_kit/env.h>
#include <lx_emul/log.h>

extern "C" void lx_emul_print_string(char const *str) {
	 Lx_kit::env().console.print_string(str); }
