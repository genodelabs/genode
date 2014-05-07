/*
 * \brief  Client-side PD session interface
 * \author Norman Feske
 * \date   2012-08-15
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__LINUX_PD_SESSION__CLIENT_H_
#define _INCLUDE__LINUX_PD_SESSION__CLIENT_H_

#include <linux_pd_session/linux_pd_session.h>
#include <base/rpc_client.h>

namespace Genode {

	struct Linux_pd_session_client : Rpc_client<Linux_pd_session>
	{
		explicit Linux_pd_session_client(Capability<Linux_pd_session> session)
		: Rpc_client<Linux_pd_session>(session) { }

		int bind_thread(Thread_capability thread) {
			return call<Rpc_bind_thread>(thread); }

		int assign_parent(Parent_capability parent) {
			return call<Rpc_assign_parent>(parent); }


		/*****************************
		 * Linux-specific extension **
		 *****************************/

		void start(Capability<Dataspace> binary) {
			call<Rpc_start>(binary); }
	};
}

#endif /* _INCLUDE__LINUX_PD_SESSION__CLIENT_H_ */
