/*
 * \brief  CPU driver for core
 * \author Martin Stein
 * \date   2012-04-23
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _ODROID_XU__PROCESSOR_DRIVER_H_
#define _ODROID_XU__PROCESSOR_DRIVER_H_

/* core includes */
#include <processor_driver/cortex_a15.h>

namespace Genode
{
	/**
	 * CPU driver for core
	 */
	class Cpu : public Cortex_a15::Cpu
	{
		public:

			/**
			 * Return kernel name of the executing processor
			 */
			static unsigned id() { return 0; }

			/**
			 * Return kernel name of the primary processor
			 */
			static unsigned primary_id() { return primary_id(); }
	};
}

#endif /* _ODROID_XU__PROCESSOR_DRIVER_H_ */

