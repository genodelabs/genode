/*
 * \brief  Compile-time-defined parent-service registry
 * \author Norman Feske
 * \date   2017-01-23
 *
 * Child-management utilities such as 'Slave::Policy' take a registry of
 * permitted parent services as construction argument. As a special form of
 * such a registry, a 'Static_parent_services' object is statically defined at
 * compile time instead of populated during runtime. It thereby allows the
 * creation of a parent-service registry without the need for dynamic memory
 * allocations if the types of the registered services are known at compile
 * time.
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__OS__STATIC_PARENT_SERVICES_H_
#define _INCLUDE__OS__STATIC_PARENT_SERVICES_H_

#include <base/registry.h>
#include <base/service.h>

namespace Genode { template <typename...> class Static_parent_services; }


template <typename... SESSION_TYPES>
class Genode::Static_parent_services : public Registry<Registered<Parent_service> >
{
	private:

		template <typename...>
		struct Service_recursive;

		template <typename HEAD, typename... TAIL>
		struct Service_recursive<HEAD, TAIL...>
		{
			Registered<Parent_service> service;
			Service_recursive<TAIL...> tail;

			Service_recursive(Registry<Registered<Parent_service> > &registry)
			: service(registry, HEAD::service_name()), tail(registry) { }
		};

		template <typename LAST>
		struct Service_recursive<LAST>
		{
			Registered<Parent_service> service;

			Service_recursive(Registry<Registered<Parent_service> > &registry)
			: service(registry, LAST::service_name()) { }
		};

		Service_recursive<SESSION_TYPES...> _service_recursive { *this };
};

#endif /* _INCLUDE__OS__STATIC_PARENT_SERVICES_H_ */
