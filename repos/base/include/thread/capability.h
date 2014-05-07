/*
 * \brief  Thread capability type
 * \author Norman Feske
 * \date   2008-08-16
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__THREAD__CAPABILITY_H_
#define _INCLUDE__THREAD__CAPABILITY_H_

#include <base/capability.h>

namespace Genode {

	/*
	 * The 'Thread_capability' type is created by the CPU session.
	 * Hence, we use the CPU session's 'Cpu_thread' as association.
	 */
	class Cpu_thread;
	typedef Capability<Cpu_thread> Thread_capability;
}

#endif /* _INCLUDE__THREAD__CAPABILITY_H_ */
