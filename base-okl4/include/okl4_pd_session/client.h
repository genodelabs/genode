/*
 * \brief  Client-side OKL4 specific pd session interface
 * \author Stefan Kalkowski
 * \date   2009-06-03
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__OKL4_PD_SESSION__CLIENT_H_
#define _INCLUDE__OKL4_PD_SESSION__CLIENT_H_

#include <pd_session/client.h>
#include <okl4_pd_session/okl4_pd_session.h>

namespace Genode {

	struct Okl4_pd_session_client : Rpc_client<Okl4_pd_session>
	{
		explicit Okl4_pd_session_client(Pd_session_capability cap)
		: Rpc_client<Okl4_pd_session>(static_cap_cast<Okl4_pd_session>(cap)) { }

		int bind_thread(Thread_capability thread) {
			return call<Rpc_bind_thread>(thread); }

		int assign_parent(Parent_capability parent) {
			return call<Rpc_assign_parent>(parent); }

		Okl4::L4_SpaceId_t space_id() {
			return call<Rpc_space_id>(); }

		void space_pager(Thread_capability thread) {
			call<Rpc_space_pager>(thread); }
	};
}

#endif /* _INCLUDE__OKL4_PD_SESSION__CLIENT_H_ */
