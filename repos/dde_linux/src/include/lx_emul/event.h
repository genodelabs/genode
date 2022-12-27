/*
 * \brief  Lx_emul support for input events
 * \author Christian Helmuth
 * \date   2022-06-24
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _LX_EMUL__EVENT_H_
#define _LX_EMUL__EVENT_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Translate keycodes from Linux to Genode to prevent future incompatibilities
 */
extern unsigned lx_emul_event_keycode(unsigned linux_keycode);

#ifdef __cplusplus
}
#endif

#endif /* _LX_EMUL__USB_H_ */
