/*
 * \brief  PD service provided to Noux processes
 * \author Christian Prochaska
 * \date   2016-05-23
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _NOUX__LOCAL_PD_SERVICE_H_
#define _NOUX__LOCAL_PD_SERVICE_H_

/* Genode includes */
#include <base/service.h>

/* Noux includes */
#include <pd_session_component.h>

namespace Noux {

	class Local_pd_service : public Service
	{
		private:

			Rpc_entrypoint         &_ep;
			Pd_session_capability  _cap;

		public:

			Local_pd_service(Rpc_entrypoint &ep, Pd_session_capability cap)
			:
				Service(Pd_session::service_name()), _ep(ep),
				_cap(cap)
			{ }

			Genode::Session_capability session(const char *args, Affinity const &) override
			{
				warning(__func__, " not implemented");
				return Genode::Session_capability();
			}

			void upgrade(Genode::Session_capability, const char *args) override
			{
				env()->parent()->upgrade(_cap, args);
			}

			void close(Genode::Session_capability session) override
			{
				warning(__func__, " not implemented");
			}
	};
}

#endif /* _NOUX__LOCAL_PD_SERVICE_H_ */
