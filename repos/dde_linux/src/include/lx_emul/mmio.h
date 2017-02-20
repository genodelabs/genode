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

/**********************
 ** asm-generic/io.h **
 **********************/

#define writeq(value, addr) (*(volatile uint64_t *)(addr) = (value))
#define writel(value, addr) (*(volatile uint32_t *)(addr) = (value))
#define writew(value, addr) (*(volatile uint16_t *)(addr) = (value))
#define writeb(value, addr) (*(volatile uint8_t *)(addr) = (value))

#define readq(addr) (*(volatile uint64_t *)(addr))
#define readl(addr) (*(volatile uint32_t *)(addr))
#define readw(addr) (*(volatile uint16_t *)(addr))
#define readb(addr) (*(volatile uint8_t  *)(addr))
