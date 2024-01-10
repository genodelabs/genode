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

#ifndef _CORE__SPEC__ARM__VIRTUALIZATION__GICV2_H_
#define _CORE__SPEC__ARM__VIRTUALIZATION__GICV2_H_

#include <hw/spec/arm/gicv2.h>

namespace Board {

	class Global_interrupt_controller { public: void init() {} };
	class Pic;
};


class Board::Pic : public Hw::Gicv2
{
	private:

		using uint32_t = Genode::uint32_t;

		struct Gich : Genode::Mmio<0x104>
		{
			struct Gich_hcr    : Register<0x00,  32> { };
			struct Gich_vmcr   : Register<0x08,  32> { };
			struct Gich_misr   : Register<0x10,  32> { };
			struct Gich_eisr0  : Register<0x20,  32> { };
			struct Gich_elrsr0 : Register<0x30,  32> { };
			struct Gich_apr    : Register<0xf0,  32> { };
			struct Gich_lr0    : Register<0x100, 32> { };

			Gich();
		} _gich {};

	public:

		struct Virtual_context
		{
			uint32_t lr    { 0 };
			uint32_t apr   { 0 };
			uint32_t vmcr  { 0x4c0000 };
			uint32_t misr  { 0 };
			uint32_t eisr  { 0 };
			uint32_t elrsr { 0xffffffff };
		};

		bool ack_virtual_irq(Virtual_context & c);
		void insert_virtual_irq(Virtual_context & c, unsigned irq);

		Pic(Global_interrupt_controller &) { }
};

#endif /* _CORE__SPEC__ARM__VIRTUALIZATION__GICV2_H_ */
