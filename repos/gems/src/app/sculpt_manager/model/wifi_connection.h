/*
 * \brief  Connection state of the wireless driver
 * \author Norman Feske
 * \date   2018-05-08
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MODEL__WIFI_CONNECTION_H_
#define _MODEL__WIFI_CONNECTION_H_

#include "access_point.h"

namespace Sculpt { struct Wifi_connection; }


struct Sculpt::Wifi_connection
{
	enum State { DISCONNECTED, CONNECTING, CONNECTED };

	State state;

	bool auth_failed;

	Access_point::Bssid bssid;
	Access_point::Ssid  ssid;

	/**
	 * Create 'Wifi_connection' object from 'state' report
	 */
	static Wifi_connection from_xml(Xml_node node)
	{
		bool const connected =
			node.has_sub_node("accesspoint") &&
			node.sub_node("accesspoint").attribute("state").has_value("connected");
		bool const connecting =
			node.has_sub_node("accesspoint") &&
			node.sub_node("accesspoint").attribute("state").has_value("connecting");
		bool const auth_failed =
			node.has_sub_node("accesspoint") &&
			node.sub_node("accesspoint").attribute_value("auth_failure", false);

		if (!connected && !connecting)
			return { .state = DISCONNECTED,
			         .auth_failed = auth_failed,
			         .bssid = Access_point::Bssid{},
			         .ssid  = Access_point::Bssid{} };

		Xml_node const ap = node.sub_node("accesspoint");

		return { .state = connected ? CONNECTED : CONNECTING,
		         .auth_failed = false,
		         .bssid = ap.attribute_value("bssid", Access_point::Bssid()),
		         .ssid  = ap.attribute_value("ssid",  Access_point::Ssid()) };
	}

	static Wifi_connection disconnected_wifi_connection()
	{
		return Wifi_connection { DISCONNECTED, false, Access_point::Bssid{}, Access_point::Ssid{} };
	}

	bool connected()    const { return state == CONNECTED;  }
	bool connecting()   const { return state == CONNECTING; }
	bool auth_failure() const { return auth_failed;         }
};

#endif /* _MODEL__WIFI_CONNECTION_H_ */
