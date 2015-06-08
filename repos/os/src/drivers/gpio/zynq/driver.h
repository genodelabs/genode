/*
 * \brief  Gpio Driver for Zynq
 * \author Mark Albers
 * \date   2015-03-12
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _DRIVER_H_
#define _DRIVER_H_

#include <io_mem_session/connection.h>
#include <timer_session/connection.h>
#include <util/mmio.h>
#include <vector>
#include <new>
#include <platform/zynq/drivers/board_base.h>
#include "gpio.h"

namespace Gpio {
	using namespace Genode;
	class Driver;
}

class Gpio::Driver
{
    private:

        std::vector<Zynq_Gpio*> _gpio_bank;

        Driver(std::vector<Genode::addr_t> addr)
        {
            for (unsigned i = 0; i < addr.size(); i++)
            {
                _gpio_bank.push_back(new Zynq_Gpio(addr[i], Genode::Board_base::GPIO_MMIO_SIZE));
            }
        }

        ~Driver()
        {
            for (std::vector<Zynq_Gpio*>::iterator it = _gpio_bank.begin() ; it != _gpio_bank.end(); ++it)
            {
                delete (*it);
            }
            _gpio_bank.clear();
        }

	public:

        static Driver& factory(std::vector<Genode::addr_t> addr);

        Genode::uint8_t read(unsigned gpio, bool isChannel2)
        {
            Zynq_Gpio *gpio_reg = _gpio_bank[gpio];
            return gpio_reg->gpio_read(isChannel2);
        }

        bool write(unsigned gpio, Genode::uint8_t data, bool isChannel2)
        {
            Zynq_Gpio *gpio_reg = _gpio_bank[gpio];
            return gpio_reg->gpio_write(data, isChannel2);
        }
};

Gpio::Driver& Gpio::Driver::factory(std::vector<Genode::addr_t> addr)
{
    static Gpio::Driver driver(addr);
    return driver;
}

#endif /* _DRIVER_H_ */
