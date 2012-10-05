/**
 * \brief  Base class for shared interrupts on x86
 * \author Sebastian Sumpf
 * \date   2012-10-05
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__X86__IRQ_PROXY_COMPONENT_H_
#define _CORE__INCLUDE__X86__IRQ_PROXY_COMPONENT_H_

#include <irq_proxy.h>

namespace Genode {
	typedef Irq_proxy<Thread<1024 * sizeof(addr_t)> > Irq_proxy_base;
}

#endif /* _CORE__INCLUDE__X86__IRQ_PROXY_COMPONENT_H_ */

