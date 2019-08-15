/*
 * \brief   ARM specific platform implementations
 * \author  Adrian-Ken Rueegsegger
 * \date    2015-03-18
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <platform.h>

using namespace Genode;

void Platform::_init_io_port_alloc() { };

void Platform::_init_additional_platform_info(Genode::Xml_generator&) { }

long Platform::irq(long const user_irq) { return user_irq; }

bool Platform::get_msi_params(const addr_t /* mmconf */, addr_t & /* address */,
                              addr_t & /* data */, unsigned & /* irq_number */)
{
	return false;
}
