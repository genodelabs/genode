/*
 * \brief  Lx_emul support for time
 * \author Stefan Kalkowski
 * \date   2021-04-30
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _LX_EMUL__TIME_H_
#define _LX_EMUL__TIME_H_

#ifdef __cplusplus
extern "C" {
#endif

void lx_emul_time_init(void);

void lx_emul_time_event(unsigned long evt);

void lx_emul_time_stop(void);

unsigned long long lx_emul_time_counter(void);

void lx_emul_time_handle(void);

void lx_emul_time_update_jiffies(void);

void lx_emul_time_initial(unsigned long long);

#ifdef __cplusplus
}
#endif

#endif /* _LX_EMUL__TIME_H_ */
