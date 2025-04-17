/*
 * \brief   Platform specific parts of CPU session
 * \author  Martin Stein
 * \date    2012-04-17
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <dataspace/capability.h>

/* core includes */
#include <cpu_session_component.h>
#include <kernel/configuration.h>

/* base-internal includes */
#include <base/internal/native_utcb.h>

using namespace Core;


Dataspace_capability Cpu_thread_component::utcb()
{
	return _platform_thread.utcb();
}


Cpu_session::Quota Cpu_session_component::quota() { return Quota(); }


size_t Cpu_session_component::_utcb_quota_size() {
	return sizeof(Native_utcb); }
