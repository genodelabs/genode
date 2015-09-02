/*
 * \brief  L2 outer cache controller ARM PL310
 * \author Johannes Schlatow
 * \author Stefan Kalkowski
 * \author Martin Stein
 * \date   2014-06-02
 */

/*
 * Copyright (C) 2014-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _SPEC__ARM__PL310_H_
#define _SPEC__ARM__PL310_H_

/* Genode includes */
#include <util/mmio.h>

namespace Arm
{
	using Genode::addr_t;

	class Pl310;
}

/**
 * L2 outer cache controller
 */
class Arm::Pl310 : Genode::Mmio
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

			static access_t init_value()
			{
				access_t v = 0;
				Associativity::set(v, 1);
				Way_size::set(v, 3);
				Share_override::set(v, 1);
				Reserved::set(v, 1);
				Ns_lockdown::set(v, 1);
				Ns_irq_ctrl::set(v, 1);
				Data_prefetch::set(v, 1);
				Inst_prefetch::set(v, 1);
				Early_bresp::set(v, 1);
				return v;
			}
		};

		struct Irq_mask                : Register <0x214, 32> { };
		struct Irq_clear               : Register <0x220, 32> { };
		struct Cache_sync              : Register <0x730, 32> { };
		struct Invalidate_by_way       : Register <0x77c, 32> { };
		struct Clean_invalidate_by_way : Register <0x7fc, 32> { };

		inline void _sync() { while (read<Cache_sync>()) ; }

		void _init()
		{
			write<Irq_mask>(0);
			write<Irq_clear>(~0);
		}

	public:

		Pl310(addr_t const base) : Mmio(base) { }

		void flush()
		{
			write<Clean_invalidate_by_way>((1 << 16) - 1);
			_sync();
		}

		void invalidate()
		{
			write<Invalidate_by_way>((1 << 16) - 1);
			_sync();
		}
};

#endif /* _SPEC__ARM__PL310_H_ */
