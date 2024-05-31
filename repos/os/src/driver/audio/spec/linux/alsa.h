/*
 * \brief  ALSA-based audio for Linux API
 * \author Sebastian Sumpf
 * \author Christian Helmuth
 * \date   2009-12-04
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DRIVERS__AUDIO__SPEC__LINUX__ALSA_H_
#define _DRIVERS__AUDIO__SPEC__LINUX__ALSA_H_

#ifdef __cplusplus
extern "C" {
#endif

int audio_drv_init(char const * const);
int audio_drv_play(void *data, int frame_cnt);
void audio_drv_stop(void);
void audio_drv_start(void);

#ifdef __cplusplus
}
#endif

#endif /* _DRIVERS__AUDIO__SPEC__LINUX__ALSA_H_ */
