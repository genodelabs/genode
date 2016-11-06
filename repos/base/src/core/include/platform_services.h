/*
 * \brief  Platform-specific services
 * \author Stefan Kalkowski
 * \date   2012-10-26
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__PLATFORM_SERVICES_H_
#define _CORE__INCLUDE__PLATFORM_SERVICES_H_

#include <base/service.h>

namespace Genode {

	class Rpc_entrypoint;
	class Sliced_heap;


	/**
	 * Register platform-specific services at entrypoint, and service
	 * registry
	 *
	 * \param ep   entrypoint used for session components of platform-services
	 * \param md   metadata allocator for session components
	 * \param reg  registry where to add platform-specific services
	 */
	void platform_add_local_services(Rpc_entrypoint *ep,
	                                 Sliced_heap *md,
	                                 Registry<Service> *reg);
}

#endif /* _CORE__INCLUDE__PLATFORM_SERVICES_H_ */
