/**
 * \brief  Dummy definitions of Linux Kernel functions
 * \author Alexander Boettcher
 * \date   2022-07-01
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */


#ifdef __cplusplus
extern "C" {
#endif

void lx_emul_time_udelay(unsigned long usec);

#ifdef __cplusplus
}
#endif
