/*
 * \brief  ALSA-based audio for Linux
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

#include <alsa/asoundlib.h>

#include "alsa.h"

static snd_pcm_t *playback_handle;

int audio_drv_init(char const * const device)
{
	unsigned int rate = 44100;
	int err;
	snd_pcm_hw_params_t *hw_params;

	if ((err = snd_pcm_open(&playback_handle, device, SND_PCM_STREAM_PLAYBACK, 0)) < 0)
		return -1;

	if ((err = snd_pcm_hw_params_malloc(&hw_params)) < 0)
		return -2;

	if ((err = snd_pcm_hw_params_any(playback_handle, hw_params)) < 0)
		return -3;

	if ((err = snd_pcm_hw_params_set_access(playback_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0)
		return -4;

	if ((err = snd_pcm_hw_params_set_format(playback_handle, hw_params, SND_PCM_FORMAT_S16_LE)) < 0)
		return -5;

	if ((err = snd_pcm_hw_params_set_rate(playback_handle, hw_params, rate, 0)) < 0)
		return -6;

	if ((err = snd_pcm_hw_params_set_channels(playback_handle, hw_params, 2)) < 0)
		return -7;

	if ((err = snd_pcm_hw_params_set_period_size(playback_handle, hw_params, 2048, 0)) < 0)
		return -8;

	if ((err = snd_pcm_hw_params_set_periods(playback_handle, hw_params, 4, 0)) < 0)
		return -9;

	if ((err = snd_pcm_hw_params(playback_handle, hw_params)) < 0)
		return -10;

	snd_pcm_hw_params_free(hw_params);

	if ((err = snd_pcm_prepare(playback_handle)) < 0)
		return -11;

	return 0;
}


int audio_drv_play(void *data, int frame_cnt)
{
	int err;
	if ((err = snd_pcm_writei(playback_handle, data, frame_cnt)) != frame_cnt)
		return err;

	return 0;
}


void audio_drv_stop(void)
{
	snd_pcm_drop(playback_handle);
}


void audio_drv_start(void)
{
	snd_pcm_prepare(playback_handle);
}
