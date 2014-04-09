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

#ifndef _ARNDALE__PROCESSOR_DRIVER_H_
#define _ARNDALE__PROCESSOR_DRIVER_H_

/* core includes */
#include <processor_driver/cortex_a15.h>

namespace Genode
{
	using Cortex_a15::Processor_lazy_state;

	/**
	 * Processor driver for core
	 */
	class Processor_driver : public Cortex_a15::Processor_driver
	{
		public:

			/**
			 * Return kernel name of the executing processor
			 */
			static unsigned executing_id()
			{
				return Mpidr::Aff_0::get(Mpidr::read());
			}

			/**
			 * Return kernel name of the primary processor
			 */
			static unsigned primary_id() { return Board::PRIMARY_MPIDR_AFF_0; }
	};
}

#endif /* _ARNDALE__PROCESSOR_DRIVER_H_ */

