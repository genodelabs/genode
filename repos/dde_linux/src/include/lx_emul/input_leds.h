/*
 * \brief  Lx_emul support for input leds
 * \author Christian Helmuth
 * \date   2024-01-19
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _LX_EMUL__INPUT_LEDS_H_
#define _LX_EMUL__INPUT_LEDS_H_

#ifdef __cplusplus
extern "C" {
#endif

extern void lx_emul_input_leds_update(bool capslock, bool numlock, bool scrolllock);

#ifdef __cplusplus
}
#endif

#endif /* _LX_EMUL__INPUT_LEDS_H_ */
