/*
 * \brief  RISC-V specific config for virtio device ROM.
 * \author Piotr Tworek
 * \date   2022-06-12
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

namespace Virtdev_rom {
	enum {
		/* Taken from include/hw/riscv/virt.h in Qemu source tree. */
		NUM_VIRTIO_TRANSPORTS = 8,
		IRQ_BASE              = 1,
		/* Taken from hw/riscv/virt.c in Qemu source tree. */
		BASE_ADDRESS          = 0x10001000,
		DEVICE_SIZE           = 0x1000,
	};
}
