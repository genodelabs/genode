/*
 * \brief  CPU driver for core
 * \author Martin stein
 * \author Stefan Kalkowski
 * \date   2016-01-19
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <cpu.h>

void Genode::Fpu::init()
{
	Cpu::Cpacr::access_t cpacr = Cpu::Cpacr::read();
	Cpu::Cpacr::Cp10::set(cpacr, 3);
	Cpu::Cpacr::Cp11::set(cpacr, 3);
	Cpu::Cpacr::write(cpacr);
	_disable();
}
