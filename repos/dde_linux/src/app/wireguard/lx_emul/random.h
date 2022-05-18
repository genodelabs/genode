/**
 * \brief  Randomness generation of lx_emul
 * \author Stefan Kalkowski
 * \date   2022-01-12
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _LX_EMUL__RANDOM_H_
#define _LX_EMUL__RANDOM_H_

#ifdef __cplusplus
extern "C" {
#endif

void lx_emul_random_bytes(void * buf, int bytes);

#ifdef __cplusplus
}
#endif

#endif /* _LX_EMUL__RANDOM_H_ */
