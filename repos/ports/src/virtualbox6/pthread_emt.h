/*
 * \brief  Pthread helpers for emulation threads
 * \author Christian Helmuth
 * \date   2021-03-08
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _PTHREAD_EMT_H_
#define _PTHREAD_EMT_H_

/* Genode includes */
#include <util/interface.h>

/* local includes */
#include <sup.h>

namespace Genode { struct Entrypoint; }

namespace Pthread {

	struct Emt : Genode::Interface
	{
		virtual void switch_to_emt() = 0;
		virtual void switch_to_vcpu() = 0;

		virtual Genode::Entrypoint & genode_ep() = 0;
	};

	Emt & emt_for_cpu(Sup::Cpu_index cpu);
}

#endif /* _PTHREAD_EMT_H_ */
