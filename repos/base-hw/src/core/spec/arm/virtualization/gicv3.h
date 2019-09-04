/*
 * \brief  Gicv2 with virtualization extensions
 * \author Stefan Kalkowski
 * \date   2019-09-02
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__SPEC__ARM__VIRTUALIZATION__GICV3_H_
#define _CORE__SPEC__ARM__VIRTUALIZATION__GICV3_H_

#include <hw/spec/arm/gicv3.h>

namespace Board { class Pic; };

class Board::Pic : public Hw::Pic
{
	public:

		struct Virtual_context {
			Genode::uint64_t lr    { 0 };
			Genode::uint32_t apr   { 0 };
			Genode::uint32_t vmcr  { 0x4c0000 };
			Genode::uint32_t misr  { 0 };
			Genode::uint32_t eisr  { 0 };
			Genode::uint32_t elrsr { 0xffffffff };
		};

		bool ack_virtual_irq(Virtual_context & c)
		{
			if (!(c.eisr & 1)) return false;

			c.lr    = 0;
			c.elrsr = 0xffffffff;
			c.misr  = 0;
			c.eisr  = 0;
			return true;
		}

		void insert_virtual_irq(Virtual_context & c, unsigned irq)
		{
			enum { SPURIOUS = 1023 };

			if (irq == SPURIOUS || c.lr) return;

			c.lr     = irq | 1ULL << 41 | 1ULL << 60 | 1ULL << 62;
		}
};

#endif /* _CORE__SPEC__ARM__VIRTUALIZATION__GICV3_H_ */

