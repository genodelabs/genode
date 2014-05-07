/*
 * \brief  OKL4 specific extension of the PD session interface
 * \author Stefan Kalkowski
 * \date   2009-06-03
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__OKL4_PD_SESSION__OKL4_PD_SESSION_H_
#define _INCLUDE__OKL4_PD_SESSION__OKL4_PD_SESSION_H_

/* OKL4 includes */
namespace Okl4 { extern "C" {
#include <l4/types.h>
} }

/* Genode includes */
#include <pd_session/pd_session.h>

namespace Genode {

	struct Okl4_pd_session : Pd_session
	{
		virtual ~Okl4_pd_session() { }

		/**
		 * Get the OKL4 specific space ID of the PD
		 *
		 * Should be used only by OKLinux, as it will be removed
		 * in the future!
		 *
		 * \return the space ID
		 */
		virtual Okl4::L4_SpaceId_t space_id() = 0;

		/**
		 * Set the thread/space allowed to page the PD
		 *
		 * (have a look at SpaceControl in OKL4)
		 * Should be used only by OKLinux, as it will be removed
		 * in the future!
		 */
		virtual void space_pager(Thread_capability) = 0;


		/*********************
		 ** RPC declaration **
		 *********************/

		GENODE_RPC(Rpc_space_id, Okl4::L4_SpaceId_t, space_id);
		GENODE_RPC(Rpc_space_pager, void, space_pager, Thread_capability);

		GENODE_RPC_INTERFACE_INHERIT(Pd_session, Rpc_space_id, Rpc_space_pager);
	};
}

#endif /* _INCLUDE__OKL4_PD_SESSION__OKL4_PD_SESSION_H_ */
