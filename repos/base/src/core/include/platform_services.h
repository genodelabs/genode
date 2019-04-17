/*
 * \brief  Platform-specific services
 * \author Stefan Kalkowski
 * \date   2012-10-26
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__PLATFORM_SERVICES_H_
#define _CORE__INCLUDE__PLATFORM_SERVICES_H_

#include <core_service.h>
#include <trace/source_registry.h>

namespace Genode {

	class Rpc_entrypoint;
	class Sliced_heap;


	/**
	 * Register platform-specific services at entrypoint, and service
	 * registry
	 *
	 * \param ep    entrypoint used for session components of platform-services
	 * \param md    metadata allocator for session components
	 * \param reg   registry where to add platform-specific services
	 * \param trace registry where to add trace subjects
	 */
	void platform_add_local_services(Rpc_entrypoint         &ep,
	                                 Sliced_heap            &md,
	                                 Registry<Service>      &reg,
	                                 Trace::Source_registry &trace);
}

#endif /* _CORE__INCLUDE__PLATFORM_SERVICES_H_ */
