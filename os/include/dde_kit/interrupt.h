/*
 * \brief   Hardware-interrupt subsystem
 * \author  Christian Helmuth
 * \date    2008-08-15
 *
 * The DDE kit supports registration of one handler function per interrupt. If
 * any specific DDE implementation needs to register more than one handler,
 * multiplexing has to be implemented there!
 */

/*
 * Copyright (C) 2008-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__DDE_KIT__INTERRUPT_H_
#define _INCLUDE__DDE_KIT__INTERRUPT_H_

/**
 * Attach to hardware interrupt
 *
 * \param irq          IRQ number to attach to
 * \param shared       set to 1 if interrupt sharing is supported; set to 0
 *                     otherwise
 * \param thread_init  called just after DDE kit internal init and before any
 *                     other function
 * \param handler      IRQ handler for interrupt irq
 * \param priv         private token (argument for thread_init and handler)
 *
 * \return             attachment state
 * \retval 0           success
 * \retval !0          error
 */
int dde_kit_interrupt_attach(int irq, int shared, void(*thread_init)(void *),
                             void(*handler)(void *), void *priv);

/**
 * Detach from a previously attached interrupt.
 *
 * \param irq          IRQ number
 */
void dde_kit_interrupt_detach(int irq);

/**
 * Block interrupt.
 *
 * \param irq          IRQ number to block
 */
void dde_kit_interrupt_disable(int irq);

/**
 * Enable interrupt.
 *
 * \param irq          IRQ number to block
 */
void dde_kit_interrupt_enable(int irq);

#endif /* _INCLUDE__DDE_KIT__INTERRUPT_H_ */
