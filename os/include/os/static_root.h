/*
 * \brief  Root component for singleton services
 * \author Norman Feske
 * \date   2012-10-05
 *
 * Many components, in particular device drivers, support only one client
 * at a time. In this case, one single session may be created right at the
 * start of the program and handed out via the 'Root::session' function.
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__OS__STATIC_ROOT_H_
#define _INCLUDE__OS__STATIC_ROOT_H_

/* Genode includes */
#include <base/rpc_server.h>
#include <session/session.h>
#include <root/root.h>


namespace Genode { template <typename> class Static_root; }


/**
 * Root interface that hands out a statically created session
 */
template <typename SESSION>
class Genode::Static_root : public Genode::Rpc_object<Genode::Typed_root<SESSION> >
{
	private:

		Capability<SESSION> _session;

	public:

		/**
		 * Constructor
		 *
		 * \param session  session to be provided to the client
		 */
		Static_root(Capability<SESSION> session) : _session(session) { }


		/********************
		 ** Root interface **
		 ********************/

		Capability<Session> session(Root::Session_args const &args)
		{
			return _session;
		}

		void upgrade(Capability<Session>, Root::Upgrade_args const &) { }

		void close(Capability<Session>) { }
};

#endif /* _INCLUDE__OS__STATIC_ROOT_H_ */
