/*
 * \brief  Root interface to timer service
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \date   2006-08-15
 */

/*
 * Copyright (C) 2006-2011 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _TIMER_ROOT_H_
#define _TIMER_ROOT_H_

#include <util/arg_string.h>
#include <util/list.h>
#include <base/printf.h>
#include <base/rpc_server.h>
#include <root/root.h>
#include <cap_session/cap_session.h>

#include "timer_session_component.h"


namespace Timer {

	class Root_component : public Genode::Rpc_object<Genode::Typed_root<Session> >
	{
		private:

			Genode::Allocator              *_md_alloc;
			Genode::Cap_session            *_cap_session;
			Genode::List<Session_component> _sessions;

		public:

			/**
			 * Constructor
			 */
			Root_component(Genode::Rpc_entrypoint *session_ep,
			               Genode::Allocator      *md_alloc,
			               Genode::Cap_session    *cap)
			: _md_alloc(md_alloc), _cap_session(cap) { }


			/********************
			 ** Root interface **
			 ********************/

			Genode::Session_capability session(Root::Session_args const &args)
			{
				using namespace Genode;

				if (!args.is_valid_string()) throw Invalid_args();

				size_t ram_quota = Arg_string::find_arg(args.string(), "ram_quota").ulong_value(0);
				size_t require = sizeof(Session_component) + STACK_SIZE;
				if (ram_quota < require) {
					PWRN("Insufficient donated ram_quota (%zd bytes), require %zd bytes",
					     ram_quota, require);
				}

				/* create session object */
				Session_component *sc = new (_md_alloc) Session_component(_cap_session);
				_sessions.insert(sc);
				return sc->cap();
			}

			void upgrade(Genode::Session_capability, Root::Upgrade_args const &) { }

			void close(Genode::Session_capability session_cap)
			{
				/*
				 * Lookup session-component by its capability by asking each
				 * entry point for object belonging to the capability. Only one
				 * entry point is expected to give the right answer.
				 */
				Session_component *sc = _sessions.first();
				for (; sc && !sc->belongs_to(session_cap); sc = sc->next());

				if (!sc) {
					PWRN("attempted to close non-existing session");
					return;
				}

				_sessions.remove(sc);
				destroy(_md_alloc, sc);
			}
	};
}

#endif
