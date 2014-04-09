/*
 * \brief  Processor driver for core
 * \author Martin Stein
 * \date   2012-12-14
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _IMX53__PROCESSOR_DRIVER_H_
#define _IMX53__PROCESSOR_DRIVER_H_

/* core includes */
#include <processor_driver/cortex_a8.h>

namespace Genode
{
	using Cortex_a8::Processor_lazy_state;

	/**
	 * Processor driver for core
	 */
	class Processor_driver : public Cortex_a8::Processor_driver
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

#endif /* _IMX53__PROCESSOR_DRIVER_H_ */

