/*
 * \brief  Dummy implemntation of platform-specific services
 * \author Stefan Kalkowski
 * \date   2012-10-26
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <platform_services.h>


void Core::platform_add_local_services(Rpc_entrypoint &, Sliced_heap &,
                                       Registry<Service> &,
                                       Trace::Source_registry &,
                                       Ram_allocator &) { }
