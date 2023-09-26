/*
 * \brief  Interface to get access to privileged system control capability
 * \author Alexander Boettcher
 * \date   2023-09-25
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__SYSTEM_CONTROL_H_
#define _CORE__INCLUDE__SYSTEM_CONTROL_H_


#include <base/rpc_server.h>


namespace Core {

	class System_control;

	System_control & init_system_control(Allocator &, Rpc_entrypoint &);
}


class Core::System_control : Interface
{
	public:

		virtual Capability<Pd_session::System_control> control_cap(Affinity::Location) const = 0;
};

#endif /* _CORE__INCLUDE__SYSTEM_CONTROL_H_ */
