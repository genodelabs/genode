/*
 * \brief  Client-side loader-session interface
 * \author Christian Prochaska
 * \date   2009-10-05
 */

/*
 * Copyright (C) 2009-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__LOADER_SESSION__CLIENT_H_
#define _INCLUDE__LOADER_SESSION__CLIENT_H_

#include <loader_session/loader_session.h>
#include <loader_session/capability.h>
#include <base/rpc_client.h>
#include <os/alarm.h>

namespace Loader {

	struct Session_client : Genode::Rpc_client<Session>
	{
		Session_client(Loader::Session_capability session)
		: Genode::Rpc_client<Session>(session) { }


		/******************************
		 ** Loader-session interface **
		 ******************************/

		Genode::Dataspace_capability dataspace() {
			return call<Rpc_dataspace>(); }

		void start(Start_args const &args,
		           int max_width, int max_height,
		           Genode::Alarm::Time timeout,
		           Name const &name = "")
		{
			call<Rpc_start>(args, max_width, max_height, timeout, name);
		}

		Nitpicker::View_capability view(int *w, int *h, int *buf_x, int *buf_y)
		{
			int dummy = 0;
			return call<Rpc_view>(w ? w : &dummy,
			                      h ? h : &dummy,
			                      buf_x ? buf_x : &dummy,
			                      buf_y ? buf_y : &dummy);
		}
	};
}

#endif /* _INCLUDE__PLUGIN_SESSION__CLIENT_H_ */
