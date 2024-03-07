/*
 * \brief  Terminal root
 * \author Christian Prochaska
 * \date   2012-05-16
 */

/*
 * Copyright (C) 2012-2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TERMINAL_ROOT_H_
#define _TERMINAL_ROOT_H_

/* Genode includes */
#include <root/component.h>

/* local includes */
#include "terminal_session_component.h"


namespace Terminal_crosslink {

	using namespace Genode;

	class Root : public Root_component<Session_component>
	{
		private:

			Session_component _session_component1, _session_component2;

			enum Session_state {
				FIRST_SESSION_OPEN  = 1 << 0,
				SECOND_SESSION_OPEN = 1 << 1
			};

			int _session_state;

		public:

			Session_capability session(Root::Session_args const &,
			                           Genode::Affinity   const &) override
			{
				if (!(_session_state & FIRST_SESSION_OPEN)) {
					_session_state |= FIRST_SESSION_OPEN;
					return _session_component1.cap();
				} else if (!(_session_state & SECOND_SESSION_OPEN)) {
					_session_state |= SECOND_SESSION_OPEN;
					return _session_component2.cap();
				}

				return Session_capability();
			}

			void upgrade(Genode::Session_capability, Root::Upgrade_args const &) override { }

			void close(Genode::Session_capability session) override
			{
				if (_session_component1.belongs_to(session))
					_session_state &= ~FIRST_SESSION_OPEN;
				else
					_session_state &= ~SECOND_SESSION_OPEN;
			}

			/**
			 * Constructor
			 */
			Root(Env &env, Allocator &alloc, size_t buffer_size)
			: Root_component(&env.ep().rpc_ep(), &alloc),
			  _session_component1(env, _session_component2, buffer_size),
			  _session_component2(env, _session_component1, buffer_size),
			  _session_state(0)
			{ }
	};
}

#endif /* _TERMINAL_ROOT_H_ */
