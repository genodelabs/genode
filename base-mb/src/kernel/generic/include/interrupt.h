/*
 * \brief  Common lowlevel interrupt handling
 * \author Martin stein
 * \date   2010-06-23
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _SRC__CORE__KERNEL__INCLUDE__INTERRUPT_H_
#define _SRC__CORE__KERNEL__INCLUDE__INTERRUPT_H_

/* OS includes */
#include <base/fixed_stdint.h>

/* kernel includes */
#include <kernel/irq_controller.h>

/* platform includes */
#include <xmb/config.h>


using namespace Genode;


/**
 * Interrupt handling level 2, calls handler if possible or nop and return
 */
void handle_interrupt();


/**
 * Globally enable all interrupts
 *
 * \param  controller  interrupt controller that shall be used by handlings
 */
void enable_interrupts(Irq_controller* const controller);


/**
 * Globally disable all irq's
 */
void disable_interrupt();


#endif /* _SRC__CORE__KERNEL__INCLUDE__INTERRUPT_H_ */
