/*
 * \brief  Representation of a wireless access point
 * \author Norman Feske
 * \date   2018-05-07
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MODEL__ACCESS_POINT_H_
#define _MODEL__ACCESS_POINT_H_

#include "types.h"

namespace Sculpt {

	struct Access_point;
	struct Access_point_update_policy;

	using Access_points = List_model<Access_point>;
};


struct Sculpt::Access_point : List_model<Access_point>::Element
{
	using Bssid = String<32>;
	using Ssid = String<32>;

	enum Protection { UNKNOWN, UNPROTECTED, WPA_PSK };

	Bssid      const bssid;
	Ssid       const ssid;
	Protection const protection;

	unsigned quality = 0;

	Access_point(Bssid const &bssid, Ssid const &ssid, Protection protection)
	: bssid(bssid), ssid(ssid), protection(protection) { }

	bool unprotected()   const { return protection == UNPROTECTED; }
	bool wpa_protected() const { return protection == WPA_PSK; }

	bool matches(Xml_node const &node) const
	{
		return node.attribute_value("ssid", Access_point::Ssid()) == ssid;
	}

	static bool type_matches(Xml_node const &node)
	{
		return node.has_type("accesspoint");
	}
};

#endif /* _MODEL__ACCESS_POINT_H_ */
