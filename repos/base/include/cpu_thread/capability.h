/*
 * \brief  Thread capability type
 * \author Norman Feske
 * \date   2016-05-11
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__CPU_THREAD__CAPABILITY_H_
#define _INCLUDE__CPU_THREAD__CAPABILITY_H_

#include <cpu_thread/cpu_thread.h>
#include <base/capability.h>

namespace Genode { typedef Capability<Cpu_thread> Cpu_thread_capability; }

#endif /* _INCLUDE__CPU_THREAD__CAPABILITY_H_ */
