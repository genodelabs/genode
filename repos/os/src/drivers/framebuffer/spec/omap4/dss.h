/*
 * \brief  General display subsystem registers
 * \author Norman Feske
 * \date   2012-06-11
 */

#ifndef _DSS_H_
#define _DSS_H_

/* Genode includes */
#include <util/mmio.h>

struct Dss : Genode::Mmio
{
	Dss(Genode::addr_t const mmio_base) : Genode::Mmio(mmio_base) { }

	struct Sysstatus : Register<0x14, 32> { };

	struct Ctrl : Register<0x40, 32>
	{
		struct Venc_hdmi_switch : Bitfield<15, 1>
		{
			enum { HDMI = 1 };
		};
	};
	struct Status : Register<0x5c, 32> { };
};

#endif /* _DSS_H_ */
