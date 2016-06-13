/*
 * \brief  Startup code and program image specifica
 * \author Christian Helmuth
 * \date   2006-05-16
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__CRT0_H_
#define _INCLUDE__BASE__CRT0_H_


/************************************
 ** Program image exported symbols **
 ************************************/

extern unsigned _prog_img_beg;  /* begin of program image (link address) */
extern unsigned _prog_img_end;  /* end of program image */

extern void (*_ctors_start)();  /* begin of constructor table */
extern void (*_ctors_end)();    /* end of constructor table */
extern void (*_dtors_start)();  /* begin of destructor table */
extern void (*_dtors_end)();    /* end of destructor table */

extern unsigned _start;         /* program entry point */
extern unsigned _stack_low;     /* lower bound of intial stack */
extern unsigned _stack_high;    /* upper bound of intial stack */


/***************************************************
 ** Parameters for parent capability construction **
 ***************************************************/

/*
 * The protection domain creator initializes the information about the parent
 * capability prior the execution of the main thread. It corresponds to the
 * '_parent_cap' symbol defined in 'src/ld/genode.ld'.
 */
extern unsigned long _parent_cap;

#endif /* _INCLUDE__BASE__CRT0_H_ */
