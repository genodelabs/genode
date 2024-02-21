/*
 * \brief  State of the current call
 * \author Norman Feske
 * \date   2022-06-29
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MODEL__CURRENT_CALL_H_
#define _MODEL__CURRENT_CALL_H_

#include <model/modem_state.h>

namespace Sculpt { struct Current_call; }


struct Sculpt::Current_call
{
	using Number = Modem_state::Number;

	Number number { };

	enum class State {

		NONE,

		/*
		 * Entered by user interaction
		 */
		ACCEPTED,   /* picked up incoming call */
		REJECTED,   /* rejected incoming or active call */
		INITIATED,
		CANCELED,   /* canced outbound call */

		/*
		 * Entered by modem activity
		 */
		INCOMING,
		HUNG_UP,    /* disconnected by callee */
		OUTBOUND,   /* dialing */
		ALERTING,   /* ring at the callee */
		ACTIVE,
	};

	State state = State::NONE;

	/**
	 * Return true if the modem state is applicable to the current state
	 */
	bool _applicable(Modem_state const &modem) const
	{
		if (state == State::NONE)
			return true;

		/* accept state updates when current state already reflects the modem */
		bool const entered_by_modem_activity = (state == State::INCOMING)
		                                    || (state == State::HUNG_UP)
		                                    || (state == State::OUTBOUND)
		                                    || (state == State::ALERTING)
		                                    || (state == State::ACTIVE);
		if (entered_by_modem_activity)
			return true;

		/* an accepted incoming call became active */
		if ((state == State::ACCEPTED) && modem.active_call())
			return true;

		/* forget last canceled initiated call when a new call comes in */
		if ((state == State::CANCELED) && modem.incoming_call())
			return true;

		/* reset canceled state once the modem cleared the call */
		if ((state == State::CANCELED) && !modem.any_call())
			return true;

		/* reset rejected state once the modem cleared the call */
		if ((state == State::REJECTED) && !modem.any_call())
			return true;

		/* clear rejected state when called from a different number */
		if ((state == State::REJECTED) && modem.incoming_call() && (modem.number() != number))
			return true;

		/* an initiated call is reflected by the modem as outbound or alerting */
		if ((state == State::INITIATED) && modem.outbound_call())
			return true;

		/* an initiated call became active */
		if ((state == State::INITIATED) && modem.active_call())
			return true;

		return false;
	}

	static State _from_modem_call_state(Modem_state::Call_state call_state)
	{
		switch (call_state) {
		case Modem_state::Call_state::INCOMING: return State::INCOMING;
		case Modem_state::Call_state::ACTIVE:   return State::ACTIVE;
		case Modem_state::Call_state::ALERTING: return State::ALERTING;
		case Modem_state::Call_state::OUTBOUND: return State::OUTBOUND;
		case Modem_state::Call_state::NONE:     break;
		};
		return State::NONE;
	}

	bool speaker = false;

	bool connecting() const
	{
		return state == State::INITIATED
		    || state == State::OUTBOUND
		    || state == State::ALERTING;
	}

	bool incoming() const { return state == State::INCOMING; }
	bool accepted() const { return state == State::ACCEPTED; }
	bool active()   const { return state == State::ACTIVE;   }
	bool none()     const { return state == State::NONE;     }
	bool canceled() const { return state == State::CANCELED; }

	void accept()
	{
		if (state == State::INCOMING) {
			state   = State::ACCEPTED;
			speaker = false;
		}
	}

	void reject()
	{
		if (state == State::INCOMING || state == State::ACTIVE)
			state =  State::REJECTED;
		speaker = false;
	}

	void initiate(Number const &n)
	{
		number  = n;
		state   = State::INITIATED;
		speaker = false;
	}

	void cancel()
	{
		state = State::CANCELED;
		speaker = false;
	}

	void toggle_speaker()
	{
		speaker = !speaker;
	}

	void update(Modem_state const &modem)
	{
		if (_applicable(modem)) {
			state  = _from_modem_call_state(modem.call_state());
			number = modem.number();
		}

		if (state == State::NONE)
			speaker = false;
	}

	void gen_modem_config(Xml_generator &xml) const
	{
		xml.attribute("speaker", speaker ? "yes" : "no");

		switch (state) {
		case State::NONE:
		case State::INCOMING:
			break;

		case State::ACCEPTED:
			xml.node("call", [&] {
				xml.attribute("number", number);
				xml.attribute("state", "accepted");
			});
			break;

		case State::REJECTED:
		case State::HUNG_UP:
		case State::CANCELED:
			xml.node("call", [&] {
				xml.attribute("number", number);
				xml.attribute("state", "rejected");
			});
			break;

		case State::INITIATED:
		case State::OUTBOUND:
		case State::ALERTING:
		case State::ACTIVE:
			xml.node("call", [&] {
				xml.attribute("number", number);
			});
			break;
		}
	}

	bool operator != (Current_call const &other) const
	{
		return (number  != other.number)
		    || (state   != other.state)
		    || (speaker != other.speaker);
	}
};

#endif /* _MODEL__CURRENT_CALL_H_ */
