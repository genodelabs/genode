/*
 * \brief  Interrupt controller for kernel
 * \author Norman Feske
 * \date   2013-04-05
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _RPI__PIC_H_
#define _RPI__PIC_H_

/* Genode includes */
#include <util/mmio.h>
#include <base/stdint.h>

/* core includes */
#include <board.h>

namespace Kernel
{
	class Pic : Genode::Mmio
	{
		struct Irq_pending_basic : Register<0x0, 32>
		{
			struct Timer : Bitfield<0, 1> { };
			struct Gpu   : Bitfield<8, 2> { };
		};

		struct Irq_pending_gpu_1 : Register<0x04, 32> { };
		struct Irq_pending_gpu_2 : Register<0x08, 32> { };
		struct Irq_enable_gpu_1  : Register<0x10, 32> { };
		struct Irq_enable_gpu_2  : Register<0x14, 32> { };

		struct Irq_enable_basic  : Register<0x18, 32> { };

		struct Irq_disable_gpu_1  : Register<0x1c, 32> { };
		struct Irq_disable_gpu_2  : Register<0x20, 32> { };
		struct Irq_disable_basic  : Register<0x24, 32> { };

		private:

			typedef Genode::uint32_t uint32_t;

			/**
			 * Return true if specified interrupt is pending
			 */
			static bool _is_pending(unsigned i, uint32_t p1, uint32_t p2)
			{
				return i < 32 ? (p1 & (1 << i)) : (p2 & (1 << (i - 32)));
			}

		public:

			Pic() : Genode::Mmio(Genode::Board::IRQ_CONTROLLER_BASE) { mask(); }

			void init_processor_local() { }

			bool take_request(unsigned &irq)
			{
				/* read basic IRQ status mask */
				uint32_t const p = read<Irq_pending_basic>();


				/* read GPU IRQ status mask */
				uint32_t const p1 = read<Irq_pending_gpu_1>(),
				               p2 = read<Irq_pending_gpu_2>();

				if (Irq_pending_basic::Timer::get(p)) {
					irq = Irq_pending_basic::Timer::SHIFT;
					return true;
				}

				/* search for lowest set bit in pending masks */
				for (unsigned i = 0; i < 64; i++) {
					if (!_is_pending(i, p1, p2))
						continue;

					irq = Genode::Board_base::GPU_IRQ_BASE + i;
					return true;
				}

				return false;
			}

			void finish_request() { }

			void unmask() { PDBG("not implemented"); }

			void mask()
			{
				write<Irq_disable_basic>(~0);
				write<Irq_disable_gpu_1>(~0);
				write<Irq_disable_gpu_2>(~0);
			}

			void unmask(unsigned const i, unsigned)
			{
				if (i < 8)
					write<Irq_enable_basic>(1 << i);
				else if (i < 32 + 8)
					write<Irq_enable_gpu_1>(1 << (i - 8));
				else
					write<Irq_enable_gpu_2>(1 << (i - 8 - 32));
			}

			void mask(unsigned const i)
			{
				if (i < 8)
					write<Irq_disable_basic>(1 << i);
				else if (i < 32 + 8)
					write<Irq_disable_gpu_1>(1 << (i - 8));
				else
					write<Irq_disable_gpu_2>(1 << (i - 8 - 32));
			}
	};
}

#endif /* _RPI__PIC_H_ */

