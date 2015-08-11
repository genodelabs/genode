/*
 * \brief  Genode audio driver backend
 * \author Josef Soentgen
 * \date   2015-05-17
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <audio_in_session/connection.h>
#include <audio_out_session/connection.h>
#include <terminal_session/connection.h>
#include <timer_session/connection.h>
#include <trace/timestamp.h>

/* VirtualBox includes */
#include "VBoxDD.h"
#include "vl_vbox.h"
extern "C" {
#include "audio.h"
}
#include <iprt/alloc.h>

#define AUDIO_CAP "genode"
extern "C" {
#include "audio_int.h"
}


static bool const verbose = false;
#define PLOGV(...) if (verbose) PLOG(__VA_ARGS__);


enum {
	AUDIO_CHANNELS   = 2,
	AUDIO_SAMPE_SIZE = sizeof(short),
	AUDIO_FREQ       = 44100,
	SIZE_PER_FRAME   = AUDIO_CHANNELS * AUDIO_SAMPE_SIZE,
};
static const char *channel_names[] = { "front left", "front right" };


struct GenodeVoiceOut
{
	HWVoiceOut             hw;
	Audio_out::Connection *audio[AUDIO_CHANNELS];
	Audio_out::Packet     *packet[AUDIO_CHANNELS];
	uint8_t               *packet_buf;
	int                    wpos;             /* in samples */
	unsigned               packets;
	int                    last_submitted;
};


struct GenodeVoiceIn {
	HWVoiceIn             hw;
	Audio_in::Connection *audio;
	uint8_t              *pcm_buf;
	int                   pcm_rpos;   /* in bytes */
	uint8_t              *packet_buf;
	int                   wpos;       /* in samples */
	int                   rpos;       /* in samples */
	unsigned              packets;
};


static inline int16_t *advance_buffer(void *buffer, int samples)
{
	enum { CHANNELS = 2 };
	return ((int16_t*)buffer) + (samples * CHANNELS);
}


static inline void copy_samples(void *dst, void const *src, int samples) {
	Genode::memcpy(dst, src, samples * SIZE_PER_FRAME); }


static int write_samples(GenodeVoiceOut *out, int16_t *src, int samples)
{
	int16_t *buf = (int16_t*)out->packet_buf;

	/* fill and submit packet */
	if (out->wpos >= Audio_out::PERIOD) {
		/* alloc new packet */
		while (true) {
			Audio_out::Connection *c = out->audio[0];
			try {
				out->packet[0] = c->stream()->alloc();
				break;
			} catch (Audio_out::Stream::Alloc_failed) {
				c->wait_for_alloc();
			}
		}

		unsigned const pos = out->audio[0]->stream()->packet_position(out->packet[0]);
		out->packet[1]     = out->audio[1]->stream()->get(pos);

		/* copy */
		float *left_content  = out->packet[0]->content();
		float *right_content = out->packet[1]->content();

		for (int i = 0; i < Audio_out::PERIOD; i++) {
			left_content[i]  = (float)(buf[i * AUDIO_CHANNELS + 0]) / 32768.0f;
			right_content[i] = (float)(buf[i * AUDIO_CHANNELS + 1]) / 32768.0f;
		}

		/* submit */
		for (int i = 0; i < AUDIO_CHANNELS; i++) {
			out->audio[i]->submit(out->packet[i]);
			out->packet[i] = nullptr;
		}
		out->last_submitted = pos;

		/* move remaining samples if any to front of buffer */
		int const remaining_samples = out->wpos - Audio_out::PERIOD;
		if (remaining_samples) {
			copy_samples(buf, advance_buffer(buf, Audio_out::PERIOD), remaining_samples);
		}
		out->wpos = remaining_samples;
		out->packets++;
	}

	/* copy new samples */
	copy_samples(advance_buffer(buf, out->wpos), src, samples);
	out->wpos += samples;

	return samples;
}


static int genode_run_out(HWVoiceOut *hw)
{
	GenodeVoiceOut *out = (GenodeVoiceOut *)hw;

	int const live = audio_pcm_hw_get_live_out(&out->hw);
	if (!live)
		return 0;

	int const samples = audio_MIN(live, out->hw.samples);

	out->hw.rpos = (out->hw.rpos + samples) % out->hw.samples;
	return samples;
}


static int genode_write(SWVoiceOut *sw, void *buf, int size)
{
	GenodeVoiceOut *out = (GenodeVoiceOut*)sw->hw;
	return write_samples(out, (int16_t*)buf, size / 4) * 4;
}


static int genode_init_out(HWVoiceOut *hw, audsettings_t *as)
{
	GenodeVoiceOut *out = (GenodeVoiceOut *)hw;

	PLOG("--- using Audio_out session ---");
	PLOGV("freq: %d", as->freq);
	PLOGV("channels: %d", as->nchannels);
	PLOGV("format: %d", as->fmt);
	PLOGV("endianness: %d", as->endianness);

	if (as->nchannels != AUDIO_CHANNELS) {
		PERR("only %d channels supported (%d were requested)",
		     AUDIO_CHANNELS, as->nchannels);
		return -1;
	}

	if (as->freq != AUDIO_FREQ) {
		PERR("only %d frequency supported (%d were requested)",
		     AUDIO_FREQ, as->freq);
		return -1;
	}

	for (int i = 0; i < AUDIO_CHANNELS; i++) {
		try {
			out->audio[i] = new (Genode::env()->heap())
				Audio_out::Connection(channel_names[i]);
		} catch (...) {
			PERR("could not establish Audio_out connection");
			while (--i > 0)
				Genode::destroy(Genode::env()->heap(), out->audio[i]);
			return -1;
		}

		out->packet[i] = nullptr;
	}

	out->wpos           = 0;
	out->last_submitted = 0;
	out->packets        = 0;

	audio_pcm_init_info(&out->hw.info, as);
	out->hw.samples    = Audio_out::PERIOD;

	size_t const bytes = (out->hw.samples << out->hw.info.shift);
	out->packet_buf    = (uint8_t*)RTMemAllocZ(2 * bytes);

	return 0;
}


static void genode_fini_out(HWVoiceOut *hw)
{
	GenodeVoiceOut *out = (GenodeVoiceOut *)hw;

	for (int i = 0; i < AUDIO_CHANNELS; i++)
		Genode::destroy(Genode::env()->heap(), out->audio[i]);

	RTMemFree((void*)out->packet_buf);
}


static int genode_ctl_out(HWVoiceOut *hw, int cmd, ...)
{
	GenodeVoiceOut *out = (GenodeVoiceOut*)hw;
	switch (cmd) {
	case VOICE_ENABLE:
		{
			PLOGV("enable playback");
			out->packets = 0;

			for (int i = 0; i < AUDIO_CHANNELS; i++) {
				out->audio[i]->stream()->reset();
				out->audio[i]->start();
			}

			break;
		}
	case VOICE_DISABLE:
		{
			PLOGV("disable playback (packets played: %u)", out->packets);

			for (int i = 0; i < AUDIO_CHANNELS; i++) {
				out->audio[i]->stop();
				out->audio[i]->stream()->invalidate_all();
				out->packet[i] = nullptr;
			}
			out->wpos = 0;

			break;
		}
	}

	return 0;
}


/***************
 ** Recording **
 ***************/

static int genode_init_in(HWVoiceIn *hw, audsettings_t *as)
{
	GenodeVoiceIn *in = (GenodeVoiceIn*)hw;

	PLOG("--- using Audio_in session ---");
	PLOGV("freq: %d", as->freq);
	PLOGV("channels: %d", as->nchannels);
	PLOGV("format: %d", as->fmt);
	PLOGV("endianness: %d", as->endianness);

	try {
		in->audio = new (Genode::env()->heap()) Audio_in::Connection("left");
	} catch (...) {
		PERR("could not establish Audio_in connection");
		return -1;
	}

	audio_pcm_init_info(&in->hw.info, as);
	in->hw.samples = Audio_in::PERIOD;

	size_t bytes   = in->hw.samples << in->hw.info.shift;
	in->pcm_buf    = (uint8_t*)RTMemAllocZ(bytes);
	in->pcm_rpos   = 0;

	in->packet_buf = (uint8_t*)RTMemAllocZ(2*bytes);
	in->rpos       = 0;
	in->wpos       = 0;
	in->packets    = 0;

	return 0;
}


static void genode_fini_in(HWVoiceIn *hw)
{
	GenodeVoiceIn *in = (GenodeVoiceIn*)hw;

	Genode::destroy(Genode::env()->heap(), in->audio);
	RTMemFree(in->pcm_buf);
	RTMemFree(in->packet_buf);
}


static int read_samples(GenodeVoiceIn *in, int16_t *dst, int samples)
{
	enum { PACKET_BUF_LEN = 2 * Audio_in::PERIOD };

	/*
	 * Acquire next Audio_in packet first and copy the content into the
	 * packet buffer.
	 */
	Audio_in::Stream &stream = *in->audio->stream();
	Audio_in::Packet *p      = stream.get(stream.pos());

	if (!p->valid())
		return 0;

	in->packets++;

	int16_t *buf = advance_buffer(in->packet_buf, in->wpos);
	float   *content = p->content();
	for (int i = 0; i < Audio_in::PERIOD; i++) {
		int16_t const v = content[i] * 32767;
		int     const j = i * 2;
		buf[j + 0]      = v;
		buf[j + 1]      = v;
	}

	p->invalidate();
	p->mark_as_recorded();
	stream.increment_position();

	in->wpos = (in->wpos + Audio_in::PERIOD) % PACKET_BUF_LEN;

	/*
	 * Copy number of given samples from packet buffer to dst buffer.
	 */
	int remaining_samples = samples;
	if ((in->rpos + remaining_samples) > PACKET_BUF_LEN) {
		int n = PACKET_BUF_LEN - in->rpos;
		int16_t const *src = advance_buffer(in->packet_buf, in->rpos);

		copy_samples(dst, src, n);
		in->rpos = 0;

		remaining_samples -= n;
		dst                = advance_buffer(dst, n);
	}

	int16_t const *src = advance_buffer(in->packet_buf, in->rpos);
	copy_samples(dst, src, remaining_samples);

	in->rpos = (in->rpos + remaining_samples) % PACKET_BUF_LEN;
	return samples;
}


static int genode_run_in(HWVoiceIn *hw)
{
	GenodeVoiceIn *in = (GenodeVoiceIn*)hw;

	int const live = audio_pcm_hw_get_live_in(&in->hw);
	if (!(in->hw.samples - live))
		return 0;

	int const dead    = in->hw.samples - live;
	int const samples = read_samples(in, (int16_t*)in->pcm_buf, dead);

	in->hw.wpos = (in->hw.wpos + samples) % in->hw.samples;
	return samples;
}


static int genode_read(SWVoiceIn *sw, void *buf, int size)
{
	GenodeVoiceIn *in = (GenodeVoiceIn*)sw->hw;

	void const *src = in->pcm_buf + in->pcm_rpos;
	Genode::memcpy(buf, src, size);
	in->pcm_rpos += size;

	/* needed by audio_pcm_hw_get_live_in() to calculate ``live'' samples */
	sw->total_hw_samples_acquired += (size / 4);

	/*
	 * This assumption totally blows if the guest uses other
	 * parameters to configure the device model.
	 */
	enum { PCM_BUFFER_SIZE = 448 /* samples */ * 2 /* ch */ * 2 /* int16_t */ };
	if (in->pcm_rpos >= PCM_BUFFER_SIZE) {
		in->pcm_rpos = 0;
	}

	return size;
}


static int genode_ctl_in(HWVoiceIn *hw, int cmd, ...)
{
	GenodeVoiceIn *in = (GenodeVoiceIn*)hw;
	switch (cmd) {
	case VOICE_ENABLE:
		{
			PLOGV("enable recording");
			in->audio->start();
			break;
		}
	case VOICE_DISABLE:
		{
			PLOGV("disable recording");
			in->audio->stop();
			in->wpos = 0;
			in->rpos = 0;
			in->pcm_rpos = 0;
			break;
		}
	}

	return 0;
}


static void *genode_audio_init(void) {
	return &oss_audio_driver; }


static void genode_audio_fini(void *) { }


static struct audio_pcm_ops genode_pcm_ops = {
	genode_init_out,
	genode_fini_out,
	genode_run_out,
	genode_write,
	genode_ctl_out,

	genode_init_in,
	genode_fini_in,
	genode_run_in,
	genode_read,
	genode_ctl_in
};


/*
 * We claim to be the OSS driver so that we do not have to
 * patch the VirtualBox source because we already claim to
 * be FreeBSD.
 */
struct audio_driver oss_audio_driver = {
	INIT_FIELD (name           = ) "oss",
	INIT_FIELD (descr          = ) "Genode Audio_out/Audio_in",
	INIT_FIELD (options        = ) NULL,
	INIT_FIELD (init           = ) genode_audio_init,
	INIT_FIELD (fini           = ) genode_audio_fini,
	INIT_FIELD (pcm_ops        = ) &genode_pcm_ops,
	INIT_FIELD (can_be_default = ) 1,
	INIT_FIELD (max_voices_out = ) INT_MAX,
	INIT_FIELD (max_voices_in  = ) INT_MAX,
	INIT_FIELD (voice_size_out = ) sizeof(GenodeVoiceOut),
	INIT_FIELD (voice_size_in  = ) sizeof(GenodeVoiceIn)
};
