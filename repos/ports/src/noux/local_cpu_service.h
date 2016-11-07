/*
 * \brief  CPU service provided to Noux processes
 * \author Josef Soentgen
 * \date   2013-04-16
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _NOUX__LOCAL_CPU_SERVICE_H_
#define _NOUX__LOCAL_CPU_SERVICE_H_

/* Genode includes */
#include <base/service.h>

/* Noux includes */
#include <cpu_session_component.h>

namespace Noux {

	class Local_cpu_service : public Service
	{
		private:

			Rpc_entrypoint         &_ep;
			Cpu_session_capability  _cap;

		public:

			Local_cpu_service(Rpc_entrypoint &ep, Cpu_session_capability cap)
			:
				Service(Cpu_session::service_name()), _ep(ep),
				_cap(cap)
			{ }

			Genode::Session_capability session(const char *args, Affinity const &)
			{
				Genode::warning(__func__, ": implement me!");
				return Genode::Session_capability();
			}

			void upgrade(Genode::Session_capability, const char *args)
			{
				env()->parent()->upgrade(_cap, args);
			}

			void close(Genode::Session_capability session)
			{
				Genode::warning(__func__, ": implement me!");
			}
	};
}

#endif /* _NOUX__LOCAL_CPU_SERVICE_H_ */
