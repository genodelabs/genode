/*
 * \brief  Power state as provided by the power driver
 * \author Norman Feske
 * \date   2022-11-09
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MODEL__POWER_STATE_H_
#define _MODEL__POWER_STATE_H_

#include <types.h>

namespace Sculpt { struct Power_state; }


struct Sculpt::Power_state
{
	bool ac_present, battery_present, charging;

	double voltage;

	struct Battery
	{
		double charge_current, power_draw;

		unsigned remaining_capacity;

		static Battery from_xml(Xml_node const &battery)
		{
			return Battery {
				.charge_current     = battery.attribute_value("charge_current", 0.0),
				.power_draw         = battery.attribute_value("power_draw", 0.0),
				.remaining_capacity = battery.attribute_value("remaining_capacity", 0u)
			};
		}
	};

	Battery battery;

	enum class Profile { UNKNOWN, PERFORMANCE, ECONOMIC };

	Profile profile;

	unsigned brightness;

	static Power_state from_xml(Xml_node const &node)
	{
		Battery battery { };

		node.with_optional_sub_node("battery", [&] (Xml_node const &node) {
			battery = Battery::from_xml(node);
		});

		auto profile_from_xml = [] (Xml_node const &node)
		{
			auto value = node.attribute_value("power_profile", String<64>());

			if (value == "performance") return Profile::PERFORMANCE;
			if (value == "economic")    return Profile::ECONOMIC;

			return Profile::UNKNOWN;
		};

		return Power_state {
			.ac_present      = node.attribute_value("ac_present", false),
			.battery_present = node.has_sub_node("battery"),
			.charging        = node.attribute_value("charging", false),
			.voltage         = node.attribute_value("voltage", 0.0),
			.battery         = battery,
			.profile         = profile_from_xml(node),
			.brightness      = node.attribute_value("brightness", 0u),
		};
	}

	using Summary = String<128>;

	Summary summary() const
	{
		if (!battery_present)
			return "AC";

		return Summary(battery.remaining_capacity,"%", charging ? " +": "");
	}

	bool modem_present() const
	{
		/* assume presence of a modem before receiving the first report */
		bool const uncertain = !ac_present && !battery_present;
		if (uncertain)
			return true;

		return battery_present;
	}
};

#endif /* _MODEL__POWER_STATE_H_ */
