/*
 * \brief   Singlethreaded minimalistic kernel
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2013-09-30
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__KERNEL__KERNEL_H_
#define _CORE__KERNEL__KERNEL_H_

/**
 * Main routine of every kernel pass
 */
extern "C" void kernel();


namespace Kernel {

	class Pd;

	Pd  &core_pd();
}

#endif /* _CORE__KERNEL__KERNEL_H_ */
