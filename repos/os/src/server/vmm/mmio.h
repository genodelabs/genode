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

#ifndef _SRC__SERVER__VMM__MMIO_H_
#define _SRC__SERVER__VMM__MMIO_H_

#include <address_space.h>
#include <state.h>

namespace Vmm {
	class Cpu;
	class Mmio_register;
	class Mmio_device;
	class Mmio_bus;
}


class Vmm::Mmio_register : public Vmm::Address_range
{
	public:

		enum Type { RO, WO, RW };

		using Name     = String<64>;
		using Register = uint64_t;
		using Space    = Address_space;

		virtual Register read(Address_range  & access, Cpu&);
		virtual void     write(Address_range & access, Cpu&, Register value);
		virtual void     set(Register value);
		virtual Register value() const;

		Mmio_register(Name     name,
		              Type     type,
		              uint64_t start,
		              uint64_t size,
		              Space  & device,
		              Register reset_value = 0)
		: Address_range(start, size),
		  _name(name),
		  _type(type),
		  _value(reset_value)
		{
			device.add(*this);
		}

	protected:

		Name const _name;
		Type const _type;
		Register   _value;

		bool _unaligned(Address_range & access);
};


class Vmm::Mmio_device : public Vmm::Address_range
{
	public:

		using Name     = String<64>;
		using Register = uint64_t;
		using Space    = Address_space;

	private:

		Name const    _name;
		Address_space _registers {};

	public:

		Space & registers() { return _registers; }

		virtual Register read(Address_range  & access, Cpu&);
		virtual void     write(Address_range & access, Cpu&, Register value);

		void add(Mmio_register & reg);

		Mmio_device(Name     name,
		            uint64_t start,
		            uint64_t size,
		            Space  & bus)
		: Address_range(start, size), _name(name)
		{
			bus.add(*this);
		}
};


struct Vmm::Mmio_bus : Vmm::Address_space
{
	void handle_memory_access(Vcpu_state &state, Cpu &cpu);
};

#endif /* _SRC__SERVER__VMM__MMIO_H_ */
