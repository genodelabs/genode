/*
 * \brief  Modem state as retrieved from the modem driver
 * \author Norman Feske
 * \date   2022-05-20
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MODEL__MODEM_STATE_H_
#define _MODEL__MODEM_STATE_H_

namespace Sculpt { struct Modem_state; }


struct Sculpt::Modem_state
{
	enum class Power { UNAVAILABLE, OFF, STARTING_UP, ON, SHUTTING_DOWN };

	Power _power;

	using Number = String<128>;

	enum class Call_state { NONE, INCOMING, ACTIVE, OUTBOUND, ALERTING };

	Call_state _call_state = Call_state::NONE;

	Number _number;

	unsigned _startup_seconds;
	unsigned _shutdown_seconds;

	enum class Pin_state { UNKNOWN, REQUIRED, CHECKING, REJECTED, OK, PUK_NEEDED };
	Pin_state _pin_state;

	unsigned _pin_remaining_attempts;

	bool transient() const
	{
		return _power == Power::STARTING_UP
		    || _power == Power::SHUTTING_DOWN;
	}

	bool on() const
	{
		return _power == Power::ON
		    || _power == Power::STARTING_UP;
	}

	bool ready() const { return _power == Power::ON; }

	Call_state call_state() const { return _call_state; }

	Number number() const { return _number; }

	bool any_call() const { return _call_state != Call_state::NONE; }

	bool incoming_call() const { return _call_state == Call_state::INCOMING; }

	bool outbound_call() const { return _call_state == Call_state::OUTBOUND
	                                 || _call_state == Call_state::ALERTING; }

	bool active_call() const { return _call_state == Call_state::ACTIVE; }

	bool pin_required() const { return _pin_state == Pin_state::REQUIRED
	                                || _pin_state == Pin_state::REJECTED; }

	bool pin_ok()       const { return _pin_state == Pin_state::OK; }

	bool pin_rejected() const { return _pin_state == Pin_state::REJECTED; }

	using Power_message = String<128>;

	Power_message power_message() const
	{
		using Msg = Power_message;

		switch (_power) {
		case Power::STARTING_UP:   return Msg(" starting up (",   _startup_seconds,  ") ");
		case Power::SHUTTING_DOWN: return Msg(" shutting down (", _shutdown_seconds, ") ");
		case Power::ON:

			switch (_pin_state) {

			case Pin_state::REQUIRED:
				return Msg(" PIN required");

			case Pin_state::REJECTED:
				return (_pin_remaining_attempts == 1)
				     ? Msg(" PIN rejected (one more try) ")
				     : Msg(" PIN rejected (", _pin_remaining_attempts, " more tries) ");

			case Pin_state::CHECKING:
				return Msg(" checking PIN ... ");

			case Pin_state::OK:
				return Msg(" ready ");

			case Pin_state::PUK_NEEDED:
				return Msg(" PUK needed, giving up. ");

			case Pin_state::UNKNOWN:
				break;
			}
			return Msg(" unknown PIN state ");

		case Power::OFF:           return Msg(" powered off ");
		case Power::UNAVAILABLE:   break;
		}
		return Msg(" unavailable" );
	}

	bool operator != (Modem_state const &other) const
	{
		return other._power                  != _power
		    || other._number                 != _number
		    || other._call_state             != _call_state
		    || other._startup_seconds        != _startup_seconds
		    || other._shutdown_seconds       != _shutdown_seconds
		    || other._pin_state              != _pin_state
		    || other._pin_remaining_attempts != _pin_remaining_attempts;
	}

	static Modem_state from_xml(Xml_node const &node)
	{
		auto power = [&]
		{
			auto value = node.attribute_value("power", String<20>());

			if (value == "on")            return Power::ON;
			if (value == "starting up")   return Power::STARTING_UP;
			if (value == "shutting down") return Power::SHUTTING_DOWN;
			if (value == "off")           return Power::OFF;

			return Power::UNAVAILABLE;
		};

		auto pin_state = [&]
		{
			auto value = node.attribute_value("pin", String<20>());

			if (value == "required")
				return node.has_attribute("pin_remaining_attempts")
				     ? Pin_state::REJECTED : Pin_state::REQUIRED;

			if (value == "checking")   return Pin_state::CHECKING;
			if (value == "ok")         return Pin_state::OK;
			if (value == "puk needed") return Pin_state::PUK_NEEDED;

			return Pin_state::UNKNOWN;
		};

		auto number = [&]
		{
			Number result { };
			node.with_optional_sub_node("call", [&] (Xml_node const &call) {
				result = call.attribute_value("number", Number()); });
			return result;
		};

		auto call_state = [&]
		{
			Call_state result = Call_state::NONE;

			node.with_optional_sub_node("call", [&] (Xml_node const &call) {

				auto const type = call.attribute_value("state", String<20>());

				result = (type == "incoming") ? Call_state::INCOMING
				       : (type == "active")   ? Call_state::ACTIVE
				       : (type == "outbound") ? Call_state::OUTBOUND
				       : (type == "alerting") ? Call_state::ALERTING
				       :                        Call_state::NONE;
			});

			return result;
		};

		return Modem_state {
			._power                  = power(),
			._call_state             = call_state(),
			._number                 = number(),
			._startup_seconds        = node.attribute_value("startup_seconds",  0u),
			._shutdown_seconds       = node.attribute_value("shutdown_seconds", 0u),
			._pin_state              = pin_state(),
			._pin_remaining_attempts = node.attribute_value("pin_remaining_attempts", 0u),
		};
	}
};

#endif /* _MODEL__MODEM_STATE_H_ */
