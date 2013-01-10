/*
 * \brief  Core-specific instance of the PD session interface for OKL4
 * \author Christian Helmuth
 * \author Stefan Kalkowski
 * \date   2006-07-17
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__OKL4__PD_SESSION_COMPONENT_H_
#define _CORE__INCLUDE__OKL4__PD_SESSION_COMPONENT_H_

/* Genode includes */
#include <okl4_pd_session/okl4_pd_session.h>
#include <base/rpc_server.h>

/* core includes */
#include <platform.h>

namespace Genode {

	class Pd_session_component : public Rpc_object<Okl4_pd_session>
	{
		private:

			Platform_pd     _pd;
			Rpc_entrypoint *_thread_ep;

		public:

			Pd_session_component(Rpc_entrypoint *thread_ep, const char *args)
			: _thread_ep(thread_ep) { }


			/**************************
			 ** Pd session interface **
			 **************************/

			int bind_thread(Thread_capability);
			int assign_parent(Parent_capability);


			/*****************************
			 ** OKL4-specific additions **
			 *****************************/

			void space_pager(Thread_capability thread);

			Okl4::L4_SpaceId_t space_id() {
				return Okl4::L4_SpaceId(_pd.pd_id()); }
	};
}

#endif /* _CORE__INCLUDE__OKL4__PD_SESSION_COMPONENT_H_ */
