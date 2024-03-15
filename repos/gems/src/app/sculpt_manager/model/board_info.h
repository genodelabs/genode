/*
 * \brief  Board discovery information
 * \author Norman Feske
 * \date   2018-06-01
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MODEL__BOARD_INFO_H_
#define _MODEL__BOARD_INFO_H_

#include "types.h"

namespace Sculpt { struct Board_info; }

struct Sculpt::Board_info
{
	bool wifi_present;
	bool lan_present;
	bool modem_present;

	static Board_info from_xml(Xml_node const &devices)
	{
		bool wifi = false, lan = false;

		devices.for_each_sub_node("device", [&] (Xml_node const &device) {
			device.with_optional_sub_node("pci-config", [&] (Xml_node const &pci) {

				auto has_class = [&] (unsigned class_value)
				{
					return pci.attribute_value("class", 0UL) == class_value;
				};

				/* PCI class values */
				static constexpr unsigned WIFI = 0x28000,
				                          LAN  = 0x20000;

				if (has_class(WIFI)) wifi = true;
				if (has_class(LAN))  lan  = true;
			});
		});

		return {
			.wifi_present  = wifi,
			.lan_present   = lan,
			.modem_present = false
		};
	}
};

#endif /* _MODEL__BOARD_INFO_H_ */
