/*
 * \brief  CPU driver for core
 * \author Martin Stein
 * \date   2012-04-23
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _PBXA9__PROCESSOR_DRIVER_H_
#define _PBXA9__PROCESSOR_DRIVER_H_

/* core includes */
#include <processor_driver/cortex_a9.h>

namespace Genode
{
	/**
	 * CPU driver for core
	 */
	class Processor_driver : public Cortex_a9::Processor_driver
	{
		public:

			/**
			 * Return kernel name of the primary processor
			 */
			static unsigned primary_id() { return 0; }

			/**
			 * Return kernel name of the executing processor
			 */
			static unsigned executing_id() { return primary_id(); }
	};
}

namespace Kernel { typedef Genode::Processor_driver Processor_driver; }

#endif /* _PBXA9__PROCESSOR_DRIVER_H_ */

