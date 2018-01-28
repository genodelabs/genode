/*
 * \brief  Genode audio driver backend
 * \author Josef Soentgen
 * \author Alexander Boettcher
 * \date   2015-05-17
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#define LOG_GROUP LOG_GROUP_DRV_HOST_AUDIO

/* Genode includes */
#include <audio_in_session/connection.h>
#include <audio_out_session/connection.h>
#include <util/reconstructible.h>

#include <trace/timestamp.h>

/* VBox Genode specific */
#include "vmm.h"

/* VBox includes */
#include "AudioMixBuffer.h"
#include "VBoxDD.h" /* for g_DrvHostOSSAudio */
#include <iprt/uuid.h> /* for PDMIBASE_2_PDMDRV */


enum {
	VBOX_CHANNELS    = 2,
	VBOX_SAMPLE_SIZE = sizeof(int16_t),

	OUT_PACKET_NUM = 16, /* number of buffered packets */

	OUT_SAMPLES = Audio_out::PERIOD,
	IN_SAMPLES  = Audio_in::PERIOD,
};

static char const * const channel_names[] = { "front left", "front right" };

struct GenodeVoiceOut
{
	PDMAUDIOHSTSTRMOUT  pStreamOut;
	Genode::Constructible<Audio_out::Connection> audio[VBOX_CHANNELS];
	Audio_out::Packet *packet { nullptr };
	uint32_t sample_pos { 0U };
};

struct GenodeVoiceIn
{
	PDMAUDIOHSTSTRMIN  pStreamIn;
	Genode::Constructible<Audio_in::Connection> audio;
	Audio_in::Packet *packet { nullptr };
	uint32_t sample_pos { 0U };
};

typedef struct DRVHOSTOSSAUDIO
{
	PPDMDRVINS         pDrvIns;
	PDMIHOSTAUDIO      IHostAudio;
} DRVHOSTOSSAUDIO, *PDRVHOSTOSSAUDIO;

static uint32_t read_samples(GenodeVoiceIn *in,
                             PPDMAUDIOMIXBUF const mixer_buf)
{
	Audio_in::Stream &stream = *in->audio->stream();
	Audio_in::Packet *p      = in->packet;

	/* reset if packet became invalid */
	if (p && !p->valid()) {
		in->packet = p = nullptr;
		in->sample_pos = 0;
	}

	uint32_t samples = RT_MIN(AudioMixBufFree(mixer_buf),
	                          IN_SAMPLES - in->sample_pos);
	uint32_t read_samples = 0;

	while (samples) {
		if (!p || !p->valid()) {
			/* get new packet if current is invalid */
			p = stream.get(stream.pos());
			if (!p || !p->valid())
				return read_samples;
		}

		/* convert samples */
		uint32_t const bytes = AUDIOMIXBUF_S2B(mixer_buf, samples);
		int16_t buf[bytes / sizeof(int16_t)];
		for (unsigned i = 0; i < samples; i++) {
			int16_t  const v = p->content()[in->sample_pos + i] * 32767;
			buf[(i * 2) + 0] = v;
			buf[(i * 2) + 1] = v;
		}

		/* transfer samples to mixer */
		uint32_t samples_written = 0;
		int rc = AudioMixBufWriteCirc(mixer_buf, buf, sizeof(buf),
		                              &samples_written);
		if (RT_FAILURE(rc))
			Genode::error("AudioMixBufWriteCirc failed rc=%d", rc);

		Assert(samples >= samples_written);
		read_samples += samples_written;

		/* stop processing when mixer does not take all of our samples */
		if (samples_written != samples) {
			Assert (in->sample_pos + samples_written < IN_SAMPLES);

			/* remember packet for later processing */
			in->packet = p;
			in->sample_pos += samples_written;

			return read_samples;
		}

		/* reset an previously only partial processed packet */
		if (p == in->packet) {
			in->packet = nullptr;
			in->sample_pos = 0;

			samples = RT_MIN(AudioMixBufFree(mixer_buf),
			                 IN_SAMPLES - in->sample_pos);
		} else
			samples -= samples_written;

		/* current packet is completely processed */
		p->invalidate();
		p->mark_as_recorded();
		stream.increment_position();

		/* next packet */
		p = stream.next(p);
	}

	return read_samples;
}

static uint32_t write_samples(GenodeVoiceOut *out,
                              PPDMAUDIOMIXBUF const mixer_buf)
{
	Audio_out::Connection * const c = &*out->audio[0];

	/* samples in byte available for sending out */
	uint32_t samples = RT_MIN(AudioMixBufLive(mixer_buf),
	                          OUT_SAMPLES - out->sample_pos);
	uint32_t written_samples = 0;

	while (samples)
	{
		if (c->stream()->queued() > OUT_PACKET_NUM)
			return written_samples;

		if (out->packet == nullptr) {

			try {
				out->packet = c->stream()->alloc();
			} catch (Audio_out::Stream::Alloc_failed) {
				LogWarn(("stream alloc failed\n"));
				return written_samples;
			}
		}

		/* assign the channels to audio streams */
		Audio_out::Packet *p[VBOX_CHANNELS] = { out->packet, nullptr };
		uint32_t const ppos = out->audio[0]->stream()->packet_position(p[0]);
		p[1]                = out->audio[1]->stream()->get(ppos);

		static_assert(VBOX_CHANNELS == 2, "Channel count does not match");

		/* copy */
		float * const left  = p[0]->content();
		float * const right = p[1]->content();

		/* setup convert buffer */
		uint32_t const bytes = AUDIOMIXBUF_S2B(mixer_buf, samples);
		int16_t buf[bytes / sizeof(int16_t)];

		/* read mixer data in */
		uint32_t samples_processed = 0;
		int rc = AudioMixBufReadCirc(mixer_buf, buf, bytes, &samples_processed);

		Assert(samples >= samples_processed);
		written_samples += samples_processed;

		if (RT_FAILURE(rc))
			LogWarn(("AudioMixBufReadCirc failed rc=%Rrc\n", rc));

		for (unsigned i = 0; i < samples_processed; i++, out->sample_pos++) {
			left [out->sample_pos] = (float)(buf[i * VBOX_CHANNELS + 0])
			                         / 32768.0f;
			right[out->sample_pos] = (float)(buf[i * VBOX_CHANNELS + 1])
			                         / 32768.0f;
		}

		Assert(out->sample_pos <= OUT_SAMPLES);

		/* submit package if enough samples are in */
		if (out->sample_pos == OUT_SAMPLES) {
			/* submit */
			for (unsigned i = 0; i < VBOX_CHANNELS; i++)
				out->audio[i]->submit(p[i]);

			/* reset packet information */
			out->sample_pos = 0;
			out->packet = nullptr;
		}

		/* check for additional samples */
		samples = RT_MIN(AudioMixBufLive(mixer_buf),
		                 OUT_SAMPLES - out->sample_pos);
	}

	return written_samples;
}


static DECLCALLBACK(int) drvHostOSSAudioControlIn(PPDMIHOSTAUDIO,
                                                  PPDMAUDIOHSTSTRMIN data,
                                                  PDMAUDIOSTREAMCMD cmd)
{
	LogFlowFuncEnter();

	AssertPtrReturn(data, VERR_INVALID_POINTER);
	GenodeVoiceIn * const in = reinterpret_cast<GenodeVoiceIn *>(data);

	switch (cmd) {
	case PDMAUDIOSTREAMCMD_ENABLE:
		in->audio->start();
		return VINF_SUCCESS;
	case PDMAUDIOSTREAMCMD_DISABLE:
		in->audio->stop();
		return VINF_SUCCESS;
	case PDMAUDIOSTREAMCMD_RESUME:
	case PDMAUDIOSTREAMCMD_PAUSE:
	default:
		AssertMsgFailed(("Invalid command %ld\n", cmd));
		return VERR_INVALID_PARAMETER;
	}
}

static DECLCALLBACK(int) drvHostOSSAudioControlOut(PPDMIHOSTAUDIO,
                                                   PPDMAUDIOHSTSTRMOUT data,
                                                   PDMAUDIOSTREAMCMD cmd)
{
	LogFlowFuncEnter();

	AssertPtrReturn(data, VERR_INVALID_POINTER);

	GenodeVoiceOut * const out = reinterpret_cast<GenodeVoiceOut *>(data);

	switch (cmd) {
	case PDMAUDIOSTREAMCMD_ENABLE:
		for (unsigned i = 0; i < VBOX_CHANNELS; i++)
			out->audio[i]->start();
		return VINF_SUCCESS;

	case PDMAUDIOSTREAMCMD_DISABLE:
		/* fill up unfinished packets with empty samples and push to stream */
		if (out->packet) {
			Audio_out::Packet *p[VBOX_CHANNELS] = { out->packet, nullptr };
			uint32_t const ppos = out->audio[0]->stream()->packet_position(out->packet);
			p[1]                = out->audio[1]->stream()->get(ppos);

			float * const left  = p[0]->content();
			float * const right = p[1]->content();

			memset(left , 0, (OUT_SAMPLES - out->sample_pos)
			                 * Audio_out::SAMPLE_SIZE);
			memset(right, 0, (OUT_SAMPLES - out->sample_pos)
			                 * Audio_out::SAMPLE_SIZE);

			for (unsigned i = 0; i < VBOX_CHANNELS; i++)
				out->audio[i]->submit(p[i]);

			/* reset packet information */
			out->sample_pos = 0;
			out->packet = nullptr;
		}

		/* stop further processing */
		for (unsigned i = 0; i < VBOX_CHANNELS; i++) {
			out->audio[i]->stop();
			out->audio[i]->stream()->invalidate_all();
		}
		return VINF_SUCCESS;

	case PDMAUDIOSTREAMCMD_RESUME:
	case PDMAUDIOSTREAMCMD_PAUSE:
	default:
		AssertMsgFailed(("Invalid command %ld\n", cmd));
		return VERR_INVALID_PARAMETER;
    }
}

static DECLCALLBACK(int) drvHostOSSAudioInit(PPDMIHOSTAUDIO)
{
	LogFlowFuncEnter();
	return VINF_SUCCESS;
}

static DECLCALLBACK(int) drvHostOSSAudioCaptureIn(PPDMIHOSTAUDIO,
                                                  PPDMAUDIOHSTSTRMIN data,
                                                  uint32_t * const samples)
{
	AssertPtrReturn(data, VERR_INVALID_POINTER);
	GenodeVoiceIn * const in = reinterpret_cast<GenodeVoiceIn *>(data);

	uint32_t const total_samples = read_samples(in, &data->MixBuf);

	if (total_samples) {
		int rc = AudioMixBufMixToParent(&data->MixBuf, total_samples, nullptr);
		if (RT_FAILURE(rc))
			LogWarn(("AudioMixBufMixToParent failed rc=%Rrc\n", rc));
	}

	if (samples)
		*samples = total_samples;

	return VINF_SUCCESS;
}

static DECLCALLBACK(int) drvHostOSSAudioFiniIn(PPDMIHOSTAUDIO,
                                               PPDMAUDIOHSTSTRMIN data)
{
	LogFlowFuncEnter();
	AssertPtrReturn(data, VERR_INVALID_POINTER);

	return VINF_SUCCESS;
}

static DECLCALLBACK(int) drvHostOSSAudioFiniOut(PPDMIHOSTAUDIO,
                                                PPDMAUDIOHSTSTRMOUT data)
{
	LogFlowFuncEnter();
	AssertPtrReturn(data, VERR_INVALID_POINTER);

	return VINF_SUCCESS;
}

static DECLCALLBACK(int) drvHostOSSAudioGetConf(PPDMIHOSTAUDIO,
                                                PPDMAUDIOBACKENDCFG cfg)
{
	cfg->cbStreamOut = sizeof(GenodeVoiceOut);
	cfg->cbStreamIn  = sizeof(GenodeVoiceIn);
	cfg->cMaxHstStrmsOut = 1;
	cfg->cMaxHstStrmsIn  = 1;

	return VINF_SUCCESS;
}

static DECLCALLBACK(int) drvHostOSSAudioInitIn(PPDMIHOSTAUDIO,
                                               PPDMAUDIOHSTSTRMIN const data,
                                               PPDMAUDIOSTREAMCFG const cfg,
                                               PPDMAUDIOSTREAMCFG,
                                               PDMAUDIORECSOURCE,
                                               uint32_t *samples)
{
	LogFlowFuncEnter();

	AssertPtrReturn(data, VERR_INVALID_POINTER);
	AssertPtrReturn(cfg,  VERR_INVALID_POINTER);

	GenodeVoiceIn * in = reinterpret_cast<GenodeVoiceIn *>(data);

	try {
		in->audio.construct(genode_env(), "left");
	} catch (...) {
		Genode::error("could not establish Audio_in connection");
        return VERR_GENERAL_FAILURE;
	}

	if (samples)
		*samples = IN_SAMPLES;

	Genode::log("--- using Audio_in session ---");
	Genode::log("freq: ",          cfg->uHz);
	Genode::log("channels: ",      cfg->cChannels);
	Genode::log("format: ",   (int)cfg->enmFormat);

	LogFlowFuncLeave();
	return VINF_SUCCESS;
}

static DECLCALLBACK(int) drvHostOSSAudioInitOut(PPDMIHOSTAUDIO,
                                                PPDMAUDIOHSTSTRMOUT const data,
                                                PPDMAUDIOSTREAMCFG const cfg,
                                                PPDMAUDIOSTREAMCFG, 
                                                uint32_t *samples)
{
	LogFlowFuncEnter();

	AssertPtrReturn(data, VERR_INVALID_POINTER);
	AssertPtrReturn(cfg,  VERR_INVALID_POINTER);

	GenodeVoiceOut * out = reinterpret_cast<GenodeVoiceOut *>(data);

	if (cfg->cChannels != VBOX_CHANNELS) {
		Genode::error("only ", (int)VBOX_CHANNELS, " channels supported ",
		              "( ", cfg->cChannels, " were requested)");
		return VERR_GENERAL_FAILURE;
	}

	if (cfg->uHz != Audio_out::SAMPLE_RATE) {
		Genode::error("only ", (int)Audio_out::SAMPLE_RATE,
		              " frequency supported (", cfg->uHz, " was requested)");
		return VERR_GENERAL_FAILURE;
	}

	for (int i = 0; i < VBOX_CHANNELS; i++) {
		try {
			out->audio[i].construct(genode_env(), channel_names[i]);
		} catch (...) {
			Genode::error("could not establish Audio_out connection");
			while (i > 0) {
				out->audio[--i].destruct();
			}
			return VERR_GENERAL_FAILURE;
		}
	}

	if (samples)
		*samples = OUT_SAMPLES;

	Genode::log("--- using Audio_out session ---");
	Genode::log("freq: ",          cfg->uHz);
	Genode::log("channels: ",      cfg->cChannels);
	Genode::log("format: ",   (int)cfg->enmFormat);

	LogFlowFuncLeave();
	return VINF_SUCCESS;
}

static DECLCALLBACK(bool) drvHostOSSAudioIsEnabled(PPDMIHOSTAUDIO, PDMAUDIODIR)
{
	return true;
}

static DECLCALLBACK(int) drvHostOSSAudioPlayOut(PPDMIHOSTAUDIO,
                                                PPDMAUDIOHSTSTRMOUT const data,
                                                uint32_t *samples)
{
	AssertPtrReturn(data, VERR_INVALID_POINTER);
	GenodeVoiceOut * const out = reinterpret_cast<GenodeVoiceOut *>(data);

	uint32_t const total_samples = write_samples(out, &data->MixBuf);
	if (total_samples)
		AudioMixBufFinish(&data->MixBuf, total_samples);

	if (samples)
		*samples = total_samples;

	return VINF_SUCCESS;
}

static DECLCALLBACK(void) drvHostOSSAudioShutdown(PPDMIHOSTAUDIO)
{
	LogFlowFuncEnter();
}

static DECLCALLBACK(void *) drvHostOSSAudioQueryInterface(PPDMIBASE pInterface,
                                                          const char *pszIID)
{
	LogFlowFuncEnter();

	PPDMDRVINS pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
	PDRVHOSTOSSAUDIO  pThis   = PDMINS_2_DATA(pDrvIns, PDRVHOSTOSSAUDIO);
	PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pDrvIns->IBase);
	PDMIBASE_RETURN_INTERFACE(pszIID, PDMIHOSTAUDIO, &pThis->IHostAudio);

	return NULL;
}

static DECLCALLBACK(int) drvHostOSSAudioConstruct(PPDMDRVINS pDrvIns,
                                                  PCFGMNODE, uint32_t)
{
	PDRVHOSTOSSAUDIO pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTOSSAUDIO);
	LogFlowFuncEnter();

	pThis->pDrvIns                   = pDrvIns;
	pDrvIns->IBase.pfnQueryInterface = drvHostOSSAudioQueryInterface;
	PDMAUDIO_IHOSTAUDIO_CALLBACKS(drvHostOSSAudio);

	return VINF_SUCCESS;
}


const PDMDRVREG g_DrvHostOSSAudio =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szName */
    "OSSAudio",
    /* szRCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "OSS audio host driver",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_AUDIO,
    /* cMaxInstances */
    ~0U,
    /* cbInstance */
    sizeof(DRVHOSTOSSAUDIO),
    /* pfnConstruct */
    drvHostOSSAudioConstruct,
    /* pfnDestruct */
    NULL,
    /* pfnRelocate */
    NULL,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    NULL,
    /* pfnSuspend */
    NULL,
    /* pfnResume */
    NULL,
    /* pfnAttach */
    NULL,
    /* pfnDetach */
    NULL,
    /* pfnPowerOff */
    NULL,
    /* pfnSoftReset */
    NULL,
    /* u32EndVersion */
    PDM_DRVREG_VERSION
};
