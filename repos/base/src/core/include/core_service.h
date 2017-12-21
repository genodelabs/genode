/*
 * \brief  Implementation of the 'Service' interface for core services
 * \author Norman Feske
 * \date   2017-05-11
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__CORE_SERVICE_H_
#define _CORE__INCLUDE__CORE_SERVICE_H_

#include <base/service.h>

namespace Genode { template <typename> struct Core_service; }


template <typename SESSION>
struct Genode::Core_service : Local_service<SESSION>
{
	Registry<Service>::Element _element;

	Core_service(Registry<Service>                        &registry,
	             typename Local_service<SESSION>::Factory &factory)
	:
		Local_service<SESSION>(factory), _element(registry, *this)
	{ }
};

#endif /* _CORE__INCLUDE__CORE_SERVICE_H_ */
