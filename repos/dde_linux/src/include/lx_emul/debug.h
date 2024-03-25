/*
 * \brief  Lx_emul support to debug Linux kernel ports
 * \author Stefan Kalkowski
 * \author Christian Helmuth
 * \date   2021-03-16
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _LX_EMUL__DEBUG_H_
#define _LX_EMUL__DEBUG_H_

#ifdef __cplusplus
extern "C" {
#endif

__attribute__((noreturn)) void lx_emul_trace_and_stop(const char * func);

void lx_emul_trace(const char * func);

__attribute__((__format__(printf, 1, 2))) void lx_emul_trace_msg(char const *fmt, ...);

void lx_emul_backtrace(void);

#ifdef __cplusplus
}
#endif

#endif /* _LX_EMUL__DEBUG_H_ */
