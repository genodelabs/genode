/*
 * \brief  VMM mmio abstractions
 * \author Stefan Kalkowski
 * \author Benjamin Lamowski
 * \date   2019-07-18
 */

/*
 * Copyright (C) 2019-2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <cpu.h>
#include <mmio.h>

using namespace Vmm;

Mmio_register::Register Mmio_register::read(Address_range & access, Cpu &)
{
	if (_type == WO)
		throw Exception("Invalid read access to register ",
		                _name, " ", access);

	using namespace Genode;

	switch (access.size()) {
	case 1: return *(uint8_t*)  ((addr_t)&_value + access.start());
	case 2: return *(uint16_t*) ((addr_t)&_value + access.start());
	case 4: return *(uint32_t*) ((addr_t)&_value + access.start());
	case 8: return _value;
	default: return 0;
	}
}


void Mmio_register::write(Address_range & access, Cpu &, Register value)
{
	if (_type == RO)
		throw Exception("Invalid write access to register ",
		                _name, " ", access);

	using namespace Genode;

	switch (access.size()) {
	case 1: *((uint8_t*)  ((addr_t)&_value + access.start())) = (uint8_t) value;
			break;
	case 2: *((uint16_t*) ((addr_t)&_value + access.start())) = (uint16_t)value;
			break;
	case 4: *((uint32_t*) ((addr_t)&_value + access.start())) = (uint32_t)value;
			break;
	case 8: _value = value;
	}
}


Mmio_register::Register Mmio_register::value() const { return _value; }


void Mmio_register::set(Register value) { _value = value; }


Mmio_device::Register Mmio_device::read(Address_range & access, Cpu & cpu)
{
	Mmio_register & reg = _registers.get<Mmio_register>(access);
	Address_range ar(access.start() - reg.start(), access.size());
	return reg.read(ar, cpu);
}


void Mmio_device::write(Address_range & access, Cpu & cpu, Register value)
{
	Mmio_register & reg = _registers.get<Mmio_register>(access);
	Address_range ar(access.start() - reg.start(), access.size());
	reg.write(ar, cpu, value);
}


void Mmio_device::add(Mmio_register & reg) { _registers.add(reg); }


void Vmm::Mmio_bus::handle_memory_access(Vcpu_state &state, Vmm::Cpu &cpu)
{
	using namespace Genode;

	struct Iss : Cpu::Esr
	{
		struct Write        : Bitfield<6,  1> {};
		struct Register     : Bitfield<16, 5> {};
		struct Sign_extend  : Bitfield<21, 1> {};
		struct Access_size  : Bitfield<22, 2> {
			enum { BYTE, HALFWORD, WORD, DOUBLEWORD }; };
		struct Valid        : Bitfield<24, 1> {};

		static bool valid(access_t v) {
			return Valid::get(v) && !Sign_extend::get(v); }

		static bool   write(access_t v) { return Write::get(v); }
		static addr_t r(access_t v) { return Register::get(v); }
	};

	Iss::access_t iss = state.esr_el2;

	if (!Iss::valid(iss))
		throw Exception("Mmio_bus: unknown ESR=", Genode::Hex(state.esr_el2));

	bool     wr    = Iss::Write::get(iss);
	unsigned idx   = (unsigned)Iss::Register::get(iss);
	uint64_t ipa   = ((uint64_t)state.hpfar_el2 << 8)
	                 + (state.far_el2 & ((1 << 12) - 1));
	uint64_t width = 1 << Iss::Access_size::get(iss);

	try {
		Address_range bus_range(ipa, width);
		Mmio_device & dev = get<Mmio_device>(bus_range);
		Address_range dev_range(ipa - dev.start(), width);
		if (wr) {
			dev.write(dev_range, cpu, state.reg(idx));
		} else {
			/* on 32-bit ARM we do not support 64-bit data access */
			state.reg(idx, (addr_t)dev.read(dev_range, cpu));
		}
	} catch(Exception & e) {
		Genode::warning(e);
		Genode::warning("Will ignore invalid bus access (IPA=",
		                Genode::Hex(ipa), ")");
	}
}
