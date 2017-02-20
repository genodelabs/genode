/*
 * \brief  Virtual Machine Monitor MMU definition
 * \author Stefan Kalkowski
 * \date   2012-06-25
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__SERVER__VMM__INCLUDE__MMU_H_
#define _SRC__SERVER__VMM__INCLUDE__MMU_H_

/* local includes */
#include <vm_state.h>
#include <ram.h>

class Mmu
{
	private:

		Genode::Vm_state &_state;
		Ram const        &_ram;

		unsigned _n_bits() { return _state.ttbrc & 0x7; }

		bool _ttbr0(Genode::addr_t mva) {
			return (!_n_bits() || !(mva >> (32 - _n_bits()))); }

		Genode::addr_t _first_level(Genode::addr_t va)
		{
			if (!_ttbr0(va))
				return ((_state.ttbr[1] & 0xffffc000) |
				        ((va >> 18) & 0xffffc));
			unsigned shift = 14 - _n_bits();
			return (((_state.ttbr[0] >> shift) << shift) |
			        (((va << _n_bits()) >> (18 + _n_bits())) & 0x3ffc));
		}

		Genode::addr_t _page(Genode::addr_t fe, Genode::addr_t va)
		{
			enum Type { FAULT, LARGE, SMALL };

			Genode::addr_t se = *((Genode::addr_t*)_ram.va(((fe & (~0UL << 10)) |
			                                                ((va >> 10) & 0x3fc))));
			switch (se & 0x3) {
			case FAULT:
				return 0;
			case LARGE:
				return ((se & (~0UL << 16)) | (va & (~0UL >> 16)));
			default:
				return ((se & (~0UL << 12)) | (va & (~0UL >> 20)));
			}
			return 0;
		}

		Genode::addr_t _section(Genode::addr_t fe, Genode::addr_t va) {
			return ((fe & 0xfff00000) | (va & 0xfffff)); }

		Genode::addr_t _supersection(Genode::addr_t fe, Genode::addr_t va)
		{
			Genode::warning(__func__, " not implemented yet!");
			return 0;
		}

	public:

		Mmu(Genode::Vm_state &state, Ram const &ram)
		: _state(state), _ram(ram) {}


		Genode::addr_t phys_addr(Genode::addr_t va)
		{
			enum Type { FAULT, PAGETABLE, SECTION };

			Genode::addr_t fe = *((Genode::addr_t*)_ram.va(_first_level(va)));
			switch (fe & 0x3) {
			case PAGETABLE:
				return _page(fe, va);
			case SECTION:
				return (fe & 0x40000) ? _supersection(fe, va)
				                      : _section(fe, va);
			}
			return 0;
		}
};

#endif /* _SRC__SERVER__VMM__INCLUDE__MMU_H_ */
