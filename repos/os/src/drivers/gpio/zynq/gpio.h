/*
 * \brief  Gpio Driver for Zynq
 * \author Mark Albers
 * \date   2015-04-02
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _GPIO_H_
#define _GPIO_H_

#include <os/attached_io_mem_dataspace.h>
#include <util/mmio.h>

namespace Gpio {
	using namespace Genode;
    class Zynq_Gpio;
}

struct Gpio::Zynq_Gpio : Attached_io_mem_dataspace, Mmio
{
    Zynq_Gpio(Genode::addr_t const mmio_base, Genode::size_t const mmio_size) :
		Genode::Attached_io_mem_dataspace(mmio_base, mmio_size),
	  	Genode::Mmio((Genode::addr_t)local_addr<void>())
    { }

    ~Zynq_Gpio()
    { }

	/*
	 * Registers
	 */

    struct GPIO_DATA : Register<0x00, 32> {};

    struct GPIO_TRI : Register<0x04, 32> {};

    struct GPIO2_DATA : Register<0x08, 32> {};

    struct GPIO2_TRI : Register<0x0C, 32> {};

    struct GIER : Register<0x011C, 32>
    {
        struct Global_Interrupt_Enable : Bitfield<31,1> {};
    };

    struct IP_IER : Register<0x0128, 32>
    {
        struct Channel_1_Interrupt_Enable : Bitfield<0,1> {};
        struct Channel_2_Interrupt_Enable : Bitfield<1,1> {};
    };

    struct IP_ISR : Register<0x120, 32>
    {
        struct Channel_1_Interrupt_Status : Bitfield<0,1> {};
        struct Channel_2_Interrupt_Status : Bitfield<1,1> {};
    };


    /*
     * Functions
     */
    Genode::uint8_t gpio_read(bool isChannel2)
    {
        if (isChannel2)
        {
            write<GPIO2_TRI>(0xffffffff);
            return read<GPIO2_DATA>();
        }
        else
        {
            write<GPIO_TRI>(0xffffffff);
            return read<GPIO_DATA>();
        }
    }

    bool gpio_write(Genode::uint8_t data, bool isChannel2)
    {
        if (isChannel2)
        {
            write<GPIO2_TRI>(0);
            write<GPIO2_DATA>(data);
        }
        else
        {
            write<GPIO_TRI>(0);
            write<GPIO_DATA>(data);
        }
        return true;
    }
};

#endif // _GPIO_H_
