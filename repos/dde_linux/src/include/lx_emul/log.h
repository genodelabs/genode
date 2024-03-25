/*
 * \brief  Lx_emul support to log messages from the kernel code
 * \author Stefan Kalkowski
 * \author Christian Helmuth
 * \date   2021-03-24
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _LX_EMUL__LOG_H_
#define _LX_EMUL__LOG_H_

#ifdef __cplusplus
extern "C" {
#endif

void lx_emul_print_string(char const *);

#ifdef __cplusplus
}
#endif

#endif /* _LX_EMUL__LOG_H_ */
