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

void Genode::Platform::_setup_io_port_alloc() { }

void Genode::Platform::setup_irq_mode(unsigned, unsigned, unsigned) { }
