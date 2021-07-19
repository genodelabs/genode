/*
 * \brief  I/O activity on top of Linux kernel functionality
 * \author Stefan Kalkowski
 * \date   2021-06-29
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _LX_USER__IO_H_
#define _LX_USER__IO_H_

#ifdef __cplusplus
extern "C" {
#endif

void lx_user_handle_io(void);

#ifdef __cplusplus
}
#endif

#endif /* _LX_USER__IO_H_ */
