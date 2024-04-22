/*
 * \brief  Global system state for suspend/resume support
 * \author Norman Feske
 * \date   2024-04-16
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MODEL__SYSTEM_STATE_H_
#define _MODEL__SYSTEM_STATE_H_

#include <types.h>

namespace Sculpt { struct System_state; }


struct Sculpt::System_state : private Noncopyable
{
	enum State {
		RUNNING,
		BLANKING, DRIVERS_STOPPING, ACPI_SUSPENDING, SUSPENDED, ACPI_RESUMING,
		POWERED_OFF, RESET
	};

	State state { RUNNING };

	static State _state_from_xml(Xml_node const &node)
	{
		auto value = node.attribute_value("state", String<64>());

		if (value == "blanking")    return State::BLANKING;
		if (value == "driver_stop") return State::DRIVERS_STOPPING;
		if (value == "s3_prepare")  return State::ACPI_SUSPENDING;
		if (value == "suspend")     return State::SUSPENDED;
		if (value == "s3_resume")   return State::ACPI_RESUMING;
		if (value == "poweroff")    return State::POWERED_OFF;
		if (value == "reset")       return State::RESET;

		return State::RUNNING;
	}

	static auto _state_name(State const state)
	{
		switch (state) {
		case RUNNING:          break;
		case BLANKING:         return "blanking";
		case DRIVERS_STOPPING: return "driver_stop";
		case ACPI_SUSPENDING:  return "s3_prepare";
		case SUSPENDED:        return "suspend";
		case ACPI_RESUMING:    return "s3_resume";
		case POWERED_OFF:      return "poweroff";
		case RESET:            return "reset";
		};
		return "";
	}

	Progress apply_config(Xml_node const &node)
	{
		State const orig = state;
		state = _state_from_xml(node);
		return { orig != state };
	}

	void generate(Xml_generator &xml) const
	{
		xml.attribute("state", _state_name(state));
	}

	bool drivers_stopping() const { return state == State::DRIVERS_STOPPING; }
	bool acpi_suspending()  const { return state == State::ACPI_SUSPENDING;  }
	bool acpi_resuming()    const { return state == State::ACPI_RESUMING;    }

	bool _acpi_completed(State const expected, Xml_node const &sleep_states) const
	{
		auto const complete = sleep_states.attribute_value("complete", String<16>());

		return (state == expected) && (complete == _state_name(expected));
	}

	bool ready_for_suspended(Xml_node const &acpi_sleep_states) const
	{
		return _acpi_completed(ACPI_SUSPENDING, acpi_sleep_states);
	}

	bool ready_for_restarting_drivers(Xml_node const &acpi_sleep_states) const
	{
		return _acpi_completed(ACPI_RESUMING, acpi_sleep_states);
	}
};

#endif /* _MODEL__SYSTEM_STATE_H_ */
