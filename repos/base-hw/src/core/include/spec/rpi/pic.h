/*
 * \brief  Programmable interrupt controller for core
 * \author Norman Feske
 * \date   2013-04-05
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__SPEC__RPI__PIC_H_
#define _CORE__INCLUDE__SPEC__RPI__PIC_H_

/* Genode includes */
#include <base/log.h>
#include <util/mmio.h>

/* core includes */
#include <board.h>

namespace Genode
{
	/**
	 * Programmable interrupt controller for core
	 */
	class Pic;

	class Usb_dwc_otg;
}


class Genode::Usb_dwc_otg : Mmio
{
	private:

		struct Core_irq_status : Register<0x14, 32>
		{
			struct Sof : Bitfield<3, 1> { };
		};

		struct Guid : Register<0x3c, 32>
		{
			struct Num : Bitfield<0, 14> { };

			/*
			 * The USB driver set 'Num' to a defined value
			 */
			struct Num_valid : Bitfield<31, 1> { };

			/*
			 * Filter is not used, overridden by the USB driver
			 */
			struct Kick : Bitfield<30, 1> { };
		};

		struct Host_frame_number : Register<0x408, 32>
		{
			struct Num : Bitfield<0, 14> { };
		};

		bool _is_sof() const
		{
			return read<Core_irq_status::Sof>();
		}

		static bool _need_trigger_sof(uint32_t host_frame,
		                              uint32_t scheduled_frame)
		{
			uint32_t const max_frame = 0x3fff;

			if (host_frame < scheduled_frame) {
				if (scheduled_frame - host_frame < max_frame / 2)
					return false;  /* scheduled frame not reached yet */
				else
					return true;   /* scheduled frame passed, host frame wrapped */
			} else {
				if (host_frame - scheduled_frame < max_frame / 2)
					return true;   /* scheduled frame passed */
				else
					return false;  /* scheduled frame wrapped, not reached */
			}
		}

	public:

		Usb_dwc_otg() : Mmio(Board::USB_DWC_OTG_BASE)
		{
			write<Guid::Num>(0);
			write<Guid::Num_valid>(false);
			write<Guid::Kick>(false);
		}

		bool handle_sof()
		{
			if (!_is_sof())
				return false;

			static int cnt = 0;

			if (++cnt == 8*20) {
				cnt = 0;
				return false;
			}

			if (!read<Guid::Num_valid>() || read<Guid::Kick>())
				return false;

			if (_need_trigger_sof(read<Host_frame_number::Num>(),
			                      read<Guid::Num>()))
				return false;

			write<Core_irq_status::Sof>(1);

			return true;
		}
};


class Genode::Pic : Mmio
{
	public:

		enum {
			NR_OF_IRQ = 64,

			/*
			 * dummy IPI value on non-SMP platform,
			 * only used in interrupt reservation within generic code
			 */
			IPI,
		};

	private:

		struct Irq_pending_basic : Register<0x0, 32>
		{
			struct Timer : Bitfield<0, 1> { };
			struct Gpu   : Bitfield<8, 2> { };
		};

		struct Irq_pending_gpu_1  : Register<0x04, 32> { };
		struct Irq_pending_gpu_2  : Register<0x08, 32> { };
		struct Irq_enable_gpu_1   : Register<0x10, 32> { };
		struct Irq_enable_gpu_2   : Register<0x14, 32> { };
		struct Irq_enable_basic   : Register<0x18, 32> { };
		struct Irq_disable_gpu_1  : Register<0x1c, 32> { };
		struct Irq_disable_gpu_2  : Register<0x20, 32> { };
		struct Irq_disable_basic  : Register<0x24, 32> { };

		Usb_dwc_otg _usb;

		/**
		 * Return true if specified interrupt is pending
		 */
		static bool _is_pending(unsigned i, uint32_t p1, uint32_t p2)
		{
			return i < 32 ? (p1 & (1 << i)) : (p2 & (1 << (i - 32)));
		}

	public:

		/**
		 * Constructor
		 */
		Pic() : Mmio(Board::IRQ_CONTROLLER_BASE) { mask(); }

		void init_cpu_local() { }

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
			for (unsigned i = 0; i < NR_OF_IRQ; i++) {
				if (!_is_pending(i, p1, p2))
					continue;

				irq = Board_base::GPU_IRQ_BASE + i;

				/* handle SOF interrupts locally, filter from the user land */
				if (irq == Board_base::DWC_IRQ)
					if (_usb.handle_sof())
						return false;

				return true;
			}

			return false;
		}

		void finish_request() { }

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

namespace Kernel { class Pic : public Genode::Pic { }; }

#endif /* _CORE__INCLUDE__SPEC__RPI__PIC_H_ */
