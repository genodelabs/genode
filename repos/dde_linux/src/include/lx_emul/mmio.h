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

#define iowmb dma_wmb
#define iormb dma_rmb

#define writeq(value, addr) ({ iowmb(); *(volatile uint64_t *)(addr) = (value); })
#define writel(value, addr) ({ iowmb(); *(volatile uint32_t *)(addr) = (value); })
#define writew(value, addr) ({ iowmb(); *(volatile uint16_t *)(addr) = (value); })
#define writeb(value, addr) ({ iowmb(); *(volatile uint8_t  *)(addr) = (value); })

#define readq(addr) ({ uint64_t const r = *(volatile uint64_t *)(addr); iormb(); r; })
#define readl(addr) ({ uint32_t const r = *(volatile uint32_t *)(addr); iormb(); r; })
#define readw(addr) ({ uint16_t const r = *(volatile uint16_t *)(addr); iormb(); r; })
#define readb(addr) ({ uint8_t  const r = *(volatile uint8_t  *)(addr); iormb(); r; })
