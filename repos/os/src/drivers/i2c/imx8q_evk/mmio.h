/*
 * \brief  I2C mmio region for platform imx8q_evk
 * \author Jean-Adrien Domage <jean-adrien.domage@gapfruit.com>
 * \date   2021-02-08
 */

/*
 * Copyright (C) 2013-2021 Genode Labs GmbH
 * Copyright (C) 2021 gapfruit AG
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _I2C_MMIO_H_
#define _I2C_MMIO_H_

#include <platform_session/device.h>

namespace I2c { struct Mmio; }


struct I2c::Mmio: Platform::Device::Mmio
{
	struct Address : Mmio::Register<0x0, 16> {
		struct Adr : Mmio::Register<0x0, 16>::Bitfield<1, 7> {};
	};

	struct Freq_divider : Mmio::Register<0x4, 16> {};

	struct Control : Mmio::Register<0x8, 16> {
		struct Repeat_start        : Mmio::Register<0x8, 16>::Bitfield<2, 1> {};
		struct Tx_ack_enable       : Mmio::Register<0x8, 16>::Bitfield<3, 1> {};
		struct Tx_rx_select        : Mmio::Register<0x8, 16>::Bitfield<4, 1> {};
		struct Master_slave_select : Mmio::Register<0x8, 16>::Bitfield<5, 1> {};
		struct Irq_enable          : Mmio::Register<0x8, 16>::Bitfield<6, 1> {};
		struct Enable              : Mmio::Register<0x8, 16>::Bitfield<7, 1> {};
	};

	struct Status : Mmio::Register<0x0C, 16> {
		struct Rcv_ack : Mmio::Register<0x0C, 16>::Bitfield<0, 1> {};
		struct Irq     : Mmio::Register<0x0C, 16>::Bitfield<1, 1> {};
		struct Srw     : Mmio::Register<0x0C, 16>::Bitfield<2, 1> {};
		struct Ial     : Mmio::Register<0x0C, 16>::Bitfield<4, 1> {};
		struct Busy    : Mmio::Register<0x0C, 16>::Bitfield<5, 1> {};
		struct Iaas    : Mmio::Register<0x0C, 16>::Bitfield<6, 1> {};
		struct Icf     : Mmio::Register<0x0C, 16>::Bitfield<7, 1> {};
	};

	struct Data : Mmio::Register<0x10, 16> {};

	Mmio(Platform::Device &device) : Platform::Device::Mmio { device } { }
};

#endif /* _I2C_MMIO_H_ */
