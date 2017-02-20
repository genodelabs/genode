/*
 * \brief   Pic implementation specific to Rpi
 * \author  Norman Feske
 * \author  Stefan Kalkowski
 * \date    2016-01-07
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <pic.h>
#include <platform.h>


bool Genode::Usb_dwc_otg::_need_trigger_sof(uint32_t host_frame,
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


Genode::Usb_dwc_otg::Usb_dwc_otg() : Mmio(Platform::mmio_to_virt(Board::USB_DWC_OTG_BASE))
{
	write<Guid::Num>(0);
	write<Guid::Num_valid>(false);
	write<Guid::Kick>(false);
}


bool Genode::Usb_dwc_otg::handle_sof()
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


Genode::Pic::Pic()
: Mmio(Platform::mmio_to_virt(Board::IRQ_CONTROLLER_BASE)) { mask(); }


bool Genode::Pic::take_request(unsigned &irq)
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


void Genode::Pic::mask()
{
	write<Irq_disable_basic>(~0);
	write<Irq_disable_gpu_1>(~0);
	write<Irq_disable_gpu_2>(~0);
}


void Genode::Pic::unmask(unsigned const i, unsigned)
{
	if (i < 8)
		write<Irq_enable_basic>(1 << i);
	else if (i < 32 + 8)
		write<Irq_enable_gpu_1>(1 << (i - 8));
	else
		write<Irq_enable_gpu_2>(1 << (i - 8 - 32));
}


void Genode::Pic::mask(unsigned const i)
{
	if (i < 8)
		write<Irq_disable_basic>(1 << i);
	else if (i < 32 + 8)
		write<Irq_disable_gpu_1>(1 << (i - 8));
	else
		write<Irq_disable_gpu_2>(1 << (i - 8 - 32));
}
