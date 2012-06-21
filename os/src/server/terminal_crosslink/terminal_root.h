/*
 * \brief  Terminal root
 * \author Christian Prochaska
 * \date   2012-05-16
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _TERMINAL_ROOT_H_
#define _TERMINAL_ROOT_H_

/* Genode includes */
#include <cap_session/cap_session.h>
#include <root/component.h>

/* local includes */
#include "terminal_session_component.h"


namespace Terminal {

	using namespace Genode;

	class Root : public Rpc_object<Typed_root<Session> >
	{
		private:

			Session_component _session_component1, _session_component2;

			enum Session_state {
				FIRST_SESSION_OPEN  = 1 << 0,
				SECOND_SESSION_OPEN = 1 << 1
			};

			int _session_state;

		public:

			Session_capability session(Root::Session_args const &args)
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

			void upgrade(Genode::Session_capability, Root::Upgrade_args const &) { }

			void close(Genode::Session_capability session)
			{
				if (_session_component1.belongs_to(session))
					_session_state &= ~FIRST_SESSION_OPEN;
				else
					_session_state &= ~SECOND_SESSION_OPEN;
			}

			/**
			 * Constructor
			 */
			Root(Rpc_entrypoint *ep, Allocator *md_alloc,
			     Cap_session &cap_session)
			: _session_component1(_session_component2, cap_session, "terminal_ep1"),
			  _session_component2(_session_component1, cap_session, "terminal_ep2"),
			  _session_state(0)
			{ }
	};
}

#endif /* _TERMINAL_ROOT_H_ */
