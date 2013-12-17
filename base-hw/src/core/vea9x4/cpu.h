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

#ifndef _VEA9X4__CPU_H_
#define _VEA9X4__CPU_H_

/* core includes */
#include <cpu/cortex_a9.h>

namespace Genode
{
	/**
	 * CPU driver for core
	 */
	class Cpu : public Cortex_a9::Cpu
	{
		public:

			/**
			 * Return kernel name of the primary processor
			 */
			static unsigned primary_id() { return 0; }

			/**
			 * Return kernel name of the executing processor
			 */
			static unsigned id() { return primary_id(); }
	};
}

#endif /* _VEA9X4__CPU_H_ */

