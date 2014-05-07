/*
 * \brief  I/0 interface used by MinATA driver
 * \author Sebastian Sumpf <Sebastian.Sumpf@genode-labs.com>
 * \date   2010-07-08
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _PIO_H_
#define _PIO_H_

#ifdef __cplusplus
extern "C" {
#endif

unsigned char pio_inbyte(unsigned char addr);
unsigned long pio_indword(unsigned char addr);
unsigned int pio_inword(unsigned char addr);

void pio_outbyte(int addr, unsigned char data);
void pio_outword(int addr, unsigned int data);
void pio_outdword(int addr, unsigned long data);

unsigned char pio_readBusMstrCmd(void);
unsigned char pio_readBusMstrStatus(void);

void pio_writeBusMstrCmd(unsigned char x);
void pio_writeBusMstrStatus(unsigned char x);
void pio_writeBusMstrPrd(unsigned long x);

/* DEBUGGING */
int printf(const char *formant, ...);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _PIO_H_ */
