/*
 * \brief   Kernel interface for inter-processor communication
 * \author  Stefan Kalkowski
 * \date    2018-11-15
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__KERNEL__SMP_H_
#define _CORE__KERNEL__SMP_H_

#include <util/interface.h>

namespace Kernel {

	/**
	 * Work that has to be propagated to a different cpu resp. core
	 */
	class Inter_processor_work;

	using Inter_processor_work_list =
		Genode::List<Genode::List_element<Inter_processor_work> >;
}


class Kernel::Inter_processor_work : Genode::Interface
{
	public:

		virtual void execute() = 0;

	protected:

		Genode::List_element<Inter_processor_work> _le { this };
};

#endif /* _CORE__KERNEL__SMP_H_ */
