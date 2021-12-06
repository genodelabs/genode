/*
 * \brief  Platform driver for Raspberry Pi 1
 * \author Stefan Kalkowski
 * \date   2021-12-06
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <mbox.h>
#include <device.h>
#include <property_command.h>
#include <property_message.h>

namespace Driver { struct Power_domains; }


struct Driver::Power_domains
{
	struct Domain : Driver::Power
	{
		Mbox         & mbox;
		unsigned const id;

		Domain(Powers    & powers,
		       Power::Name name,
		       Mbox      & mbox,
		       unsigned    id)
		:
			Power(powers, name), mbox(mbox), id(id) {}

		void _on() override
		{
			auto & msg = mbox.message<Property_message>();
			msg.append_no_response<Property_command::Set_power_state>(id,
			                                                          true,
			                                                          true);
			mbox.call<Property_message>();
		}

		void _off() override
		{
			auto & msg = mbox.message<Property_message>();
			msg.append_no_response<Property_command::Set_power_state>(id,
			                                                          false,
			                                                          true);
			mbox.call<Property_message>();
		}
	};

	Powers & powers;
	Mbox   & mbox;

	Domain sdhci  { powers, "sdhci",  mbox, 0 };
	Domain uart_0 { powers, "uart_0", mbox, 1 };
	Domain uart_1 { powers, "uart_1", mbox, 2 };
	Domain usb    { powers, "usb",    mbox, 3 };
	Domain i2c_0  { powers, "i2c_0",  mbox, 4 };
	Domain i2c_1  { powers, "i2c_1",  mbox, 5 };
	Domain i2c_2  { powers, "i2c_2",  mbox, 6 };
	Domain spi    { powers, "spi",    mbox, 7 };
	Domain ccp2tx { powers, "ccp2tx", mbox, 8 };
};
