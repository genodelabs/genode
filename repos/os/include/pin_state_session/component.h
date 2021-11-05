/*
 * \brief  Pin-state service
 * \author Norman Feske
 * \date   2021-04-20
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__PIN_STATE_SESSION__COMPONENT_H_
#define _INCLUDE__PIN_STATE_SESSION__COMPONENT_H_

#include <pin_state_session/pin_state_session.h>
#include <base/session_object.h>
#include <os/pin_driver.h>

namespace Pin_state {

	template <typename> class Session_component;

	template <typename ID>
	struct Root : Pin::Root<Session_component<ID>, Pin::Direction::IN>
	{
		using Pin::Root<Session_component<ID>, Pin::Direction::IN>::Root;
	};
}


template <typename ID>
class Pin_state::Session_component : public Session_object<Session>
{
	private:

		Pin::Assignment<ID> _assignment;

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
		 * Pin_state::Session interface
		 */
		bool state() const override
		{
			if (!_assignment.target.constructed())
				return false;

			return _assignment.driver.pin_state(_assignment.target->id);
		}

		void update_assignment()
		{
			_assignment.update(label(), Pin::Direction::IN);
		}
};

#endif /* _INCLUDE__PIN_STATE_SESSION__COMPONENT_H_ */
