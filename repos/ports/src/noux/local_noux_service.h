/*
 * \brief  Locally-provided Noux service
 * \author Norman Feske
 * \date   2011-02-17
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _NOUX__LOCAL_NOUX_SERVICE_H_
#define _NOUX__LOCAL_NOUX_SERVICE_H_

/* Genode includes */
#include <base/service.h>

namespace Noux {

	using namespace Genode;

	struct Local_noux_service : public Service
	{
		Genode::Session_capability _cap;

		/**
		 * Constructor
		 *
		 * \param cap  capability to return on session requests
		 */
		Local_noux_service(Genode::Session_capability cap)
		: Service(Session::service_name()), _cap(cap) { }

		Genode::Session_capability session(const char *args, Affinity const &)
		{
			return _cap;
		}

		void upgrade(Genode::Session_capability, const char *args) { }
		void close(Genode::Session_capability) { }
	};
}

#endif /* _NOUX__LOCAL_NOUX_SERVICE_H_ */
