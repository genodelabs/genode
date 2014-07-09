/*
 * \brief  Processor driver for core
 * \author Martin Stein
 * \date   2012-04-23
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _PROCESSOR_DRIVER_H_
#define _PROCESSOR_DRIVER_H_

/* core includes */
#include <spec/cortex_a15/processor_driver_support.h>

namespace Genode
{
	/**
	 * Processor driver for core
	 */
	class Processor_driver : public Cortex_a15
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

#endif /* _PROCESSOR_DRIVER_H_ */
