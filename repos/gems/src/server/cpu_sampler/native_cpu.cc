/*
 * \brief  Generic 'Native_cpu' setup
 * \author Christian Prochaska
 * \date   2016-05-13
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */


/* Cpu_sampler includes */
#include "cpu_session_component.h"


Genode::Capability<Genode::Cpu_session::Native_cpu>
Cpu_sampler::Cpu_session_component::_setup_native_cpu()
{
	return parent_cpu_session().native_cpu();
}


void Cpu_sampler::Cpu_session_component::_cleanup_native_cpu() { }
