/*
 * \brief   FPU implementation specific to x86_64
 * \author  Stefan Kalkowski
 * \date    2016-01-19
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <cpu.h>

void Genode::Fpu::init()
{
	Cpu::Cr0::access_t cr0_value = Cpu::Cr0::read();
	Cpu::Cr4::access_t cr4_value = Cpu::Cr4::read();

	Cpu::Cr0::Mp::set(cr0_value);
	Cpu::Cr0::Em::clear(cr0_value);
	Cpu::Cr0::Ts::set(cr0_value);
	Cpu::Cr0::Ne::set(cr0_value);
	Cpu::Cr0::write(cr0_value);

	Cpu::Cr4::Osfxsr::set(cr4_value);
	Cpu::Cr4::Osxmmexcpt::set(cr4_value);
	Cpu::Cr4::write(cr4_value);
}


void Genode::Fpu::disable() {
	Cpu::Cr0::write(Cpu::Cr0::read() | Cpu::Cr0::Ts::bits(1)); }


bool Genode::Fpu::enabled() { return !Cpu::Cr0::Ts::get(Cpu::Cr0::read()); }


bool Genode::Fpu::fault(Context &context)
{
	if (enabled()) return false;

	enable();
	if (_context == &context) return true;

	if (_context) {
		_save();
		_context->_fpu = nullptr;
	}
	_context = &context;
	_context->_fpu = this;
	_load();
	return true;
}
