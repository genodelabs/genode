/*
 * \brief  Lx_emul support to register Linux kernel initialization
 * \author Stefan Kalkowski
 * \date   2021-03-10
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _LX_EMUL__INIT_H_
#define _LX_EMUL__INIT_H_

#ifdef __cplusplus
extern "C" {
#endif

void lx_emul_initcalls(void);

void lx_emul_register_initcall(int (*initcall)(void), const char * name);

void lx_emul_start_kernel(void * dtb);

int  lx_emul_init_task_function(void * dtb);

extern void * lx_emul_init_task_struct;

#ifdef __cplusplus
}
#endif

#endif /* _LX_EMUL__INIT_H_ */
