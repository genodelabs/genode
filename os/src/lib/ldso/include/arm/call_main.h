/*
 * \brief  Call main function  (ARM specific)
 * \author Sebastian Sumpf <Sebastian.Sumpf@genode-labs.com>
 * \date   2011-05-05
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */
#ifndef _ARM__CALL_MAIN_H_
#define _ARM__CALL_MAIN_H_

/**
 * Restore SP from initial sp and jump to entry function
 */
void call_main(void (*func)(void))
{
        extern long __initial_sp;

        asm volatile ("mov %%sp, %0;"
                      "bx %1;"
                      :
                      : "r" (__initial_sp),
                        "r" (func)
                      : "memory"
                     );
}

#endif /* _ARM__CALL_MAIN_H_ */
