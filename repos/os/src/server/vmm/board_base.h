/*
 * \brief  VMM - board definitions
 * \author Stefan Kalkowski
 * \date   2019-11-13
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__SERVER__VMM__BOARD_H_
#define _SRC__SERVER__VMM__BOARD_H_

namespace Vmm {

	enum {
		GICD_MMIO_START   = 0x8000000,
		GICD_MMIO_SIZE    = 0x10000,
		GICC_MMIO_START   = 0x8010000,
		GICC_MMIO_SIZE    = 0x10000,
		GICR_MMIO_START   = 0x80a0000,
		GICR_MMIO_SIZE    = 0xf60000,

		PL011_MMIO_START  = 0x9000000,
		PL011_MMIO_SIZE   = 0x1000,
		PL011_IRQ         = 33,

		VIRTIO_MMIO_START = 0xa000000,
		VIRTIO_MMIO_SIZE  = 0x1000000,
		VIRTIO_IRQ_START  = 40,
		VIRTIO_IRQ_COUNT  = 128,

		RAM_START         = 0x40000000,
		MINIMUM_RAM_SIZE  = 32 * 1024 * 1024,

		VTIMER_IRQ        = 27,
	};
}

#endif /* _SRC__SERVER__VMM__BOARD_H_ */
