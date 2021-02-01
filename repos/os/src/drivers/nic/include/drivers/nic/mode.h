/*
 * \brief  NIC driver mode regarding the session used for packet transmission
 * \author Martin Stein
 * \date   2020-12-07
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DRIVERS__NIC__MODE_H_
#define _DRIVERS__NIC__MODE_H_

/* Genode includes */
#include <util/xml_node.h>

namespace Genode
{
	enum class Nic_driver_mode { NIC_SERVER, UPLINK_CLIENT };

	inline Nic_driver_mode read_nic_driver_mode(Xml_node const &driver_cfg)
	{
		String<16> const mode_str {
			driver_cfg.attribute_value("mode", String<16>("default")) };

		if (mode_str == "nic_server") {

			return Nic_driver_mode::NIC_SERVER;
		}
		if (mode_str == "uplink_client") {

			return Nic_driver_mode::UPLINK_CLIENT;
		}
		if (mode_str == "default") {

			return Nic_driver_mode::NIC_SERVER;
		}
		class Bad_mode { };
		throw Bad_mode { };
	}
}

#endif /* _DRIVERS__NIC__MODE_H_ */
