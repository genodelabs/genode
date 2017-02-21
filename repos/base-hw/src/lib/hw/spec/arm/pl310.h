/*
 * \brief  L2 outer cache controller ARM PL310
 * \author Johannes Schlatow
 * \author Stefan Kalkowski
 * \author Martin Stein
 * \date   2014-06-02
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__LIB__HW__SPEC__ARM__PL310_H_
#define _SRC__LIB__HW__SPEC__ARM__PL310_H_

/* Genode includes */
#include <util/mmio.h>

namespace Hw { struct Pl310; }

class Hw::Pl310 : public Genode::Mmio
{
	protected:

		struct Control : Register <0x100, 32>
		{
			struct Enable : Bitfield<0,1> { };
		};

		struct Aux : Register<0x104, 32>
		{
			struct Associativity  : Bitfield<16,1> { };
			struct Way_size       : Bitfield<17,3> { };
			struct Share_override : Bitfield<22,1> { };
			struct Reserved       : Bitfield<25,1> { };
			struct Ns_lockdown    : Bitfield<26,1> { };
			struct Ns_irq_ctrl    : Bitfield<27,1> { };
			struct Data_prefetch  : Bitfield<28,1> { };
			struct Inst_prefetch  : Bitfield<29,1> { };
			struct Early_bresp    : Bitfield<30,1> { };
		};

		struct Irq_mask                : Register <0x214, 32> { };
		struct Irq_clear               : Register <0x220, 32> { };
		struct Cache_sync              : Register <0x730, 32> { };
		struct Invalidate_by_way       : Register <0x77c, 32> { };
		struct Clean_invalidate_by_way : Register <0x7fc, 32> { };

		struct Debug : Register<0xf40, 32>
		{
			struct Dcl : Bitfield<0,1> { };
			struct Dwb : Bitfield<1,1> { };
		};

		void _sync() { while (read<Cache_sync>()) ; }

	public:

		Pl310(Genode::addr_t const base) : Mmio(base) { }

		void enable()  {}
		void disable() {}

		void clean_invalidate()
		{
			write<Clean_invalidate_by_way>((1UL << 16) - 1);
			_sync();
		}

		void invalidate()
		{
			write<Invalidate_by_way>((1UL << 16) - 1);
			_sync();
		}

		void mask_interrupts()
		{
			write<Irq_mask>(0);
			write<Irq_clear>(~0UL);
		}
};

#endif /* _SRC__LIB__HW__SPEC__ARM__PL310_H_ */
