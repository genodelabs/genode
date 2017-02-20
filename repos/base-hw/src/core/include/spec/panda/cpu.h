/*
 * \brief  CPU driver for core
 * \author Stefan Kalkowski
 * \date   2015-12-15
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CPU_H_
#define _CPU_H_

/* core includes */
#include <spec/cortex_a9/cpu_support.h>

namespace Genode { class Cpu; }

/**
 * Override the Cortex A9 Cpu driver, because some registers can be accessed
 * via the firmware running in TrustZone's secure world only
 */
class Genode::Cpu : public Genode::Cortex_a9
{
	public:

		struct Actlr : Cortex_a9::Actlr
		{
			static void enable_smp(Board&);
		};
};

#endif /* _CPU_H_ */

