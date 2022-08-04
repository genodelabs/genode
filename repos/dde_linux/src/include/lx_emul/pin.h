/*
 * \brief  Lx_emul support for accessing GPIO pins
 * \author Norman Feske
 * \date   2021-11-02
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _LX_EMUL__PIN_H_
#define _LX_EMUL__PIN_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Set output state of GPIO pin
 *
 * \pin_name  GPIO name used as label for corresponding 'Pin_control' session
 */
void lx_emul_pin_control(char const *pin_name, bool enabled);

/**
 * Get input state of GPIO pin
 *
 * \pin_name  GPIO name used as label for corresponding 'Pin_state' session
 */
int lx_emul_pin_sense(char const *pin_name);

/**
 * Request interrupt backed by an IRQ session
 */
void lx_emul_pin_irq_unmask(unsigned gic_irq, unsigned pin_irq,
                            char const *pin_name);

/**
 * Return pin IRQ number of most recently occurred pin interrupt
 *
 * This function is meant to be called by the PIO driver's interrupt handler.
 */
unsigned lx_emul_pin_last_irq(void);

/**
 * Acknowledge GPIO interrupt
 */
void lx_emul_pin_irq_ack(unsigned pin_irq);

#ifdef __cplusplus
}
#endif

#endif /* _LX_EMUL__PIN_H_ */
