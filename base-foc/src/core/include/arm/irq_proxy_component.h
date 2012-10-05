/**
 * \brief  Base class for shared interrupts on ARM
 * \author Sebastian Sumpf
 * \date   2012-10-05
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__ARM__IRQ_PROXY_COMPONENT_H_
#define _CORE__INCLUDE__ARM__IRQ_PROXY_COMPONENT_H_

#include <irq_proxy.h>

namespace Genode {

/**
 * On ARM we disable shared interrupts
 */
typedef Irq_proxy_single Irq_proxy_base;

}

#endif /* _CORE__INCLUDE__ARM__IRQ_PROXY_COMPONENT_H_ */

