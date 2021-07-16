/*
 * \brief  Linux kernel API
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \date   2014-08-21
 *
 * Based on the prototypes found in the Linux kernel's 'include/'.
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/***********************
 ** linux/irqreturn.h **
 ***********************/

typedef enum irqreturn {
	IRQ_NONE        = 0,
	IRQ_HANDLED     = 1,
	IRQ_WAKE_THREAD = 2,
} irqreturn_t;


/***********************
 ** linux/interrupt.h **
 ***********************/

typedef irqreturn_t (*irq_handler_t)(int, void *);
