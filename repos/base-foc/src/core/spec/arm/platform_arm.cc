/*
 * \brief  Platform support specific to ARM
 * \author Norman Feske
 * \date   2011-05-02
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <platform.h>

using namespace Core;


void Platform::_setup_io_port_alloc() { }


void Platform::setup_irq_mode(unsigned, unsigned, unsigned) { }


void Platform::_setup_platform_info(Xml_generator &,
                                    Foc::l4_kernel_info_t &) { }
