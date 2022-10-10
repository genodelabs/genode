/*
 * \brief  Pin-control service
 * \author Norman Feske
 * \date   2021-04-20
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__PIN_CONTROL_SESSION__COMPONENT_H_
#define _INCLUDE__PIN_CONTROL_SESSION__COMPONENT_H_

#include <pin_control_session/pin_control_session.h>
#include <base/session_object.h>
#include <os/pin_driver.h>

namespace Pin_control {

	template <typename> class Session_component;

	template <typename ID>
	struct Root : Pin::Root<Session_component<ID>, Pin::Direction::OUT>
	{
		using Pin::Root<Session_component<ID>, Pin::Direction::OUT>::Root;
	};
}


template <typename ID>
class Pin_control::Session_component : public Session_object<Session>
{
	private:

		Pin::Assignment<ID> _assignment;

		using Session_object<Session>::label;

		void _state(Pin::Level level)
		{
			if (_assignment.target.constructed())
				_assignment.driver.pin_state(_assignment.target->id, level);
		}

	public:

		using Pin_id = ID;

		Session_component(Entrypoint &ep, Resources const &resources,
		                  Label const &label, Diag &diag, Pin::Driver<ID> &driver)
		:
			Session_object<Session>(ep, resources, label, diag),
			_assignment(driver)
		{
			update_assignment();
		}

		/**
		 * Pin_control::Session interface
		 */
		void state(bool enabled) override
		{
			_state(enabled ? Pin::Level::HIGH : Pin::Level::LOW);
		}

		/**
		 * Pin_control::Session interface
		 */
		void yield() override
		{
			_state(Pin::Level::HIGH_IMPEDANCE);
		}

		void update_assignment()
		{
			_assignment.update(label(), Pin::Direction::OUT);
		}
};

#endif /* _INCLUDE__PIN_CONTROL_SESSION__COMPONENT_H_ */
