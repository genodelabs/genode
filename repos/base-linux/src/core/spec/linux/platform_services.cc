/*
 * \brief  Platform specific services for Linux
 * \author Johannes Kliemann
 * \date   2017-11-08
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 * Copyright (C) 2018 Componolit GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/service.h>

/* core includes */
#include <platform_services.h>


void Core::platform_add_local_services(Rpc_entrypoint         &,
                                       Sliced_heap            &,
                                       Registry<Service>      &,
                                       Trace::Source_registry &,
                                       Ram_allocator          &)
{ }
