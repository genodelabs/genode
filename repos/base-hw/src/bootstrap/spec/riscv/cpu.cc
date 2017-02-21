/*
 * \brief  CPU core implementation
 * \author Sebastian Sumpf
 * \author Stefan Kalkowski
 * \date   2016-02-10
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <cpu.h>

void Genode::Cpu::translation_added(addr_t const addr, size_t const size) {
	Genode::Cpu::sfence(); }
