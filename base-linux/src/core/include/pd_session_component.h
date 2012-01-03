/*
 * \brief  CORE-specific instance of the PD session interface for Linux
 * \author Norman Feske
 * \date   2006-08-14
 *
 * On Linux, we use a pd session only for keeping the information about the
 * existence of protection domains to enable us to destruct all pds of a whole
 * subtree. A pd is killed by CORE when closing the corresponding pd session.
 * The PID of the process is passed to CORE as an argument of the session
 * construction.
 */

/*
 * Copyright (C) 2006-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__LINUX__PD_SESSION_COMPONENT_H_
#define _CORE__INCLUDE__LINUX__PD_SESSION_COMPONENT_H_

/* Genode includes */
#include <util/arg_string.h>
#include <base/printf.h>
#include <base/rpc_server.h>
#include <pd_session/pd_session.h>

/* local includes */
#include "platform.h"

namespace Genode {

	class Pd_session_component : public Rpc_object<Pd_session>
	{
		private:

			unsigned long _pid;

		public:

			Pd_session_component(Rpc_entrypoint *thread_ep,
			                     const char *args);

			~Pd_session_component();


			/****************************/
			/** Pd session interface **/
			/****************************/

			/*
			 * This interface is not functional on Linux.
			 */

			int bind_thread(Thread_capability thread);
			int assign_parent(Parent_capability);
	};
}

#endif /* _CORE__INCLUDE__LINUX__PD_SESSION_COMPONENT_H_ */
