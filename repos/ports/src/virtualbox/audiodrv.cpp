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


template <size_t CAPACITY>
struct A_ring_buffer_to_bind_them
{
	size_t wpos { 0 };
	size_t rpos { 0 };

	char _data[CAPACITY];

	A_ring_buffer_to_bind_them() { }

	size_t read_avail() const
	{
		if (wpos > rpos) return wpos - rpos;
		else return (wpos - rpos + CAPACITY) % CAPACITY;
	}

	size_t write_avail() const
	{
		if (wpos > rpos) return ((rpos - wpos + CAPACITY) % CAPACITY) - 2;
		else if (wpos < rpos) return rpos - wpos;
		else return CAPACITY - 2;
	}

	size_t write(void const *src, size_t len)
	{
		size_t const avail = write_avail();
		if (avail == 0) return 0;

		size_t const limit_len = len > avail ? avail : len;
		size_t const total = wpos + len;
		size_t first, rest;

		if (total > CAPACITY) {
			first = CAPACITY - wpos;
			rest  = total % CAPACITY;
		} else {
			first = limit_len;
			rest  = 0;
		}

		Genode::memcpy(&_data[wpos], src, first);
		wpos = (wpos + first) % CAPACITY;

		if (rest) {
			Genode::memcpy(&_data[wpos], ((char const*)src) + first, rest);
			wpos = (wpos + rest) % CAPACITY;
		}

		return limit_len;
	}

	size_t read(void *dst, size_t len, bool peek = false)
	{
		size_t const avail = read_avail();
		if (avail == 0) return 0;

		size_t new_rpos = rpos;

		size_t const limit_len = len > avail ? avail : len;
		size_t const total = new_rpos + len;
		size_t first, rest;

		if (total > CAPACITY) {
			first = CAPACITY - new_rpos;
			rest  = total % CAPACITY;
		} else {
			first = limit_len;
			rest  = 0;
		}

		Genode::memcpy(dst, &_data[new_rpos], first);
		new_rpos = (new_rpos + first) % CAPACITY;

		if (rest) {
			Genode::memcpy(((char*)dst) + first, &_data[new_rpos], rest);
			new_rpos = (new_rpos + rest) % CAPACITY;
		}

		if (!peek) rpos = new_rpos;

		return limit_len;
	}

	void read_advance(size_t len) { rpos = (rpos + len) % CAPACITY; }
};


enum {
	VBOX_CHANNELS    = 2,
	VBOX_SAMPLE_SIZE = sizeof(int16_t),

	OUT_PACKET_NUM = 16, /* number of buffered packets */
	IN_PACKET_NUM  =  2, /* number of buffered in packets */

	OUT_PCM_SIZE = Audio_out::PERIOD * VBOX_SAMPLE_SIZE * VBOX_CHANNELS,
	IN_PCM_SIZE  = Audio_in::PERIOD  * VBOX_SAMPLE_SIZE * VBOX_CHANNELS,
	OUT_PCM_BUFFER_SIZE = OUT_PCM_SIZE * OUT_PACKET_NUM,
	IN_PCM_BUFFER_SIZE  = IN_PCM_SIZE  * IN_PACKET_NUM,

	OUT_PACKET_SIZE = Audio_out::PERIOD * VBOX_SAMPLE_SIZE * VBOX_CHANNELS,
	IN_PACKET_SIZE  = Audio_in::PERIOD  * VBOX_SAMPLE_SIZE * VBOX_CHANNELS,
	OUT_PACKET_BUFFER_SIZE = OUT_PACKET_SIZE * 2,
	IN_PACKET_BUFFER_SIZE  = IN_PACKET_SIZE  * 2,
};


static char const * const channel_names[] = { "front left", "front right" };


typedef A_ring_buffer_to_bind_them<OUT_PCM_BUFFER_SIZE>    Pcm_out_buffer;
typedef A_ring_buffer_to_bind_them<OUT_PACKET_BUFFER_SIZE> Out_packet_buffer;
typedef A_ring_buffer_to_bind_them<IN_PCM_BUFFER_SIZE>     Pcm_in_buffer;
typedef A_ring_buffer_to_bind_them<IN_PACKET_BUFFER_SIZE>  In_packet_buffer;


struct GenodeVoiceOut
{
	HWVoiceOut             hw;
	Audio_out::Connection *audio[VBOX_CHANNELS];
	Out_packet_buffer      packet_buf;
	Pcm_out_buffer         pcm_buf;
	unsigned               packets;
};


struct GenodeVoiceIn {
	HWVoiceIn             hw;
	Audio_in::Connection *audio;
	In_packet_buffer      packet_buf;
	Pcm_in_buffer         pcm_buf;
	unsigned              packets;
};


static int write_samples(GenodeVoiceOut *out, int16_t *src, int samples)
{
	Out_packet_buffer &packet_buf = out->packet_buf;

	/* try to fill and submit packet */
	if (packet_buf.read_avail() >= OUT_PACKET_SIZE) {
		Audio_out::Connection *c = out->audio[0];

		/* check how many submitted packets are still in the queue */
		if (c->stream()->queued() > OUT_PACKET_NUM) return 0;

		/* alloc new packets */
		Audio_out::Packet *p[2] { nullptr, nullptr };

		try { p[0] = c->stream()->alloc(); }
		catch (Audio_out::Stream::Alloc_failed) { return 0; }

		unsigned const ppos = out->audio[0]->stream()->packet_position(p[0]);
		p[1]                = out->audio[1]->stream()->get(ppos);

		/* copy */
		float *left_content  = p[0]->content();
		float *right_content = p[1]->content();

		int16_t buf[Audio_out::PERIOD*2];
		size_t const n = packet_buf.read(buf, sizeof(buf));
		if (n != sizeof(buf))
			Genode::error(__func__, ": n: ", n, " buf: ", buf);

		for (int i = 0; i < Audio_out::PERIOD; i++) {
			left_content[i]  = (float)(buf[i * VBOX_CHANNELS + 0]) / 32768.0f;
			right_content[i] = (float)(buf[i * VBOX_CHANNELS + 1]) / 32768.0f;
		}

		/* submit */
		for (int i = 0; i < VBOX_CHANNELS; i++)
			out->audio[i]->submit(p[i]);

		out->packets++;
	}

	/* copy new samples */
	int const bytes = samples * VBOX_SAMPLE_SIZE * VBOX_SAMPLE_SIZE;
	size_t const n  = packet_buf.write(src, bytes);
	return n / (VBOX_SAMPLE_SIZE * VBOX_SAMPLE_SIZE);
}


static int genode_run_out(HWVoiceOut *hw)
{
	GenodeVoiceOut * const out = (GenodeVoiceOut *)hw;
	Pcm_out_buffer & pcm_buf   = out->pcm_buf;

	int const live = audio_pcm_hw_get_live_out(&out->hw);
	if (!live)
		return 0;

	int const decr     = audio_MIN(live, out->hw.samples);
	size_t const avail = pcm_buf.read_avail();

	if ((avail / (VBOX_SAMPLE_SIZE*VBOX_CHANNELS)) < decr)
		Genode::error(__func__, ": avail: ", avail, " < decr ", decr);

	char buf[decr*VBOX_SAMPLE_SIZE*VBOX_CHANNELS];
	pcm_buf.read(buf, sizeof(buf), true);

	int const samples = write_samples(out, (int16_t*)buf, decr);
	if (samples == 0) return 0;

	pcm_buf.read_advance(samples * (VBOX_SAMPLE_SIZE*VBOX_CHANNELS));

	out->hw.rpos = (out->hw.rpos + samples) % out->hw.samples;
	return samples;
}


static int genode_write(SWVoiceOut *sw, void *buf, int size)
{
	GenodeVoiceOut * const out = (GenodeVoiceOut*)sw->hw;
	Pcm_out_buffer &pcm_buf    = out->pcm_buf;

	size_t const avail = pcm_buf.write_avail();
	if (size > avail)
		Genode::warning(__func__, ": size: ", size, " available: ", avail);

	size_t const n = pcm_buf.write(buf, size);
	if (n < size)
		Genode::warning(__func__, ": written: ", n, " expected: ", size);

	/* needed by audio_pcm_hw_get_live_out() to calculate ``live'' samples */
	sw->total_hw_samples_mixed += (size / 4);
	return size;
}


static int genode_init_out(HWVoiceOut *hw, audsettings_t *as)
{
	GenodeVoiceOut * const out = (GenodeVoiceOut *)hw;

	if (as->nchannels != VBOX_CHANNELS) {
		Genode::error("only ", (int)VBOX_CHANNELS, " channels supported ",
		              "( ", as->nchannels, " were requested)");
		return -1;
	}

	if (as->freq != Audio_out::SAMPLE_RATE) {
		Genode::error("only ", (int)Audio_out::SAMPLE_RATE, " frequency supported "
		              "(", as->freq, " was requested)");
		return -1;
	}

	for (int i = 0; i < VBOX_CHANNELS; i++) {
		try {
			out->audio[i] = new (Genode::env()->heap())
				Audio_out::Connection(channel_names[i]);
		} catch (...) {
			Genode::error("could not establish Audio_out connection");
			while (--i > 0)
				Genode::destroy(Genode::env()->heap(), out->audio[i]);
			return -1;
		}
	}

	audio_pcm_init_info(&out->hw.info, as);
	out->hw.samples = Audio_out::PERIOD;
	out->packets = 0;

	Genode::log("--- using Audio_out session ---");
	Genode::log("freq: ",       as->freq);
	Genode::log("channels: ",   as->nchannels);
	Genode::log("format: ",     (int)as->fmt);
	Genode::log("endianness: ", as->endianness);

	return 0;
}


static void genode_fini_out(HWVoiceOut *hw)
{
	GenodeVoiceOut * const out = (GenodeVoiceOut *)hw;
	for (int i = 0; i < VBOX_CHANNELS; i++)
		Genode::destroy(Genode::env()->heap(), out->audio[i]);
}


static int genode_ctl_out(HWVoiceOut *hw, int cmd, ...)
{
	GenodeVoiceOut *out = (GenodeVoiceOut*)hw;
	switch (cmd) {
	case VOICE_ENABLE:
		out->packets = 0;
		for (int i = 0; i < VBOX_CHANNELS; i++)
				out->audio[i]->start();
		break;
	case VOICE_DISABLE:
		for (int i = 0; i < VBOX_CHANNELS; i++) {
			out->audio[i]->stop();
			out->audio[i]->stream()->invalidate_all();
		}
		break;
	}
	return 0;
}


/***************
 ** Recording **
 ***************/

static int genode_init_in(HWVoiceIn *hw, audsettings_t *as)
{
	GenodeVoiceIn *in = (GenodeVoiceIn*)hw;

	try {
		in->audio = new (Genode::env()->heap()) Audio_in::Connection("left");
	} catch (...) {
		Genode::error("could not establish Audio_in connection");
		return -1;
	}

	audio_pcm_init_info(&in->hw.info, as);
	in->hw.samples = Audio_in::PERIOD;
	in->packets    = 0;

	Genode::log("--- using Audio_in session ---");
	Genode::log("freq: ",       as->freq);
	Genode::log("channels: ",   as->nchannels);
	Genode::log("format: ",     (int)as->fmt);
	Genode::log("endianness: ", as->endianness);

	return 0;
}


static void genode_fini_in(HWVoiceIn *hw)
{
	GenodeVoiceIn * const in = (GenodeVoiceIn*)hw;
	Genode::destroy(Genode::env()->heap(), in->audio);
}


static int read_samples(GenodeVoiceIn *in, int samples)
{
	In_packet_buffer &packet_buf = in->packet_buf;
	Pcm_in_buffer    &pcm_buf    = in->pcm_buf;

	while (packet_buf.read_avail() < IN_PACKET_SIZE) {
		Audio_in::Stream &stream = *in->audio->stream();
		Audio_in::Packet *p      = stream.get(stream.pos());

		if (!p->valid()) {
			if (packet_buf.read_avail() < (samples*VBOX_SAMPLE_SIZE*VBOX_CHANNELS)) {
				return 0;
			}
			break;
		}

		int16_t buf[Audio_in::PERIOD*VBOX_CHANNELS];

		if (packet_buf.write_avail() < sizeof(buf))
			return 0;

		float *content = p->content();
		for (int i = 0; i < Audio_in::PERIOD; i++) {
			int16_t const v = content[i] * 32767;
			int     const j = i * 2;
			buf[j + 0]      = v;
			buf[j + 1]      = v;
		}

		size_t const w = packet_buf.write(buf, sizeof(buf));
		if (w != sizeof(buf))
			Genode::error(__func__, ": write n: ", w, " buf: ", sizeof(buf));

		p->invalidate();
		p->mark_as_recorded();
		stream.increment_position();

		in->packets++;
		break;
	}

	int16_t buf[samples*VBOX_CHANNELS];
	size_t const r = packet_buf.read(buf, sizeof(buf), true);
	size_t const w = pcm_buf.write(buf, r);
	if (w != r)
		Genode::error(__func__, ": w: ", w, " != r: ", r);

	packet_buf.read_advance(w);

	return w / (VBOX_SAMPLE_SIZE*VBOX_CHANNELS);
}


static int genode_run_in(HWVoiceIn *hw)
{
	GenodeVoiceIn *in = (GenodeVoiceIn*)hw;

	int const live = audio_pcm_hw_get_live_in(&in->hw);
	if (!(in->hw.samples - live))
		return 0;

	int const dead    = in->hw.samples - live;
	int const samples = read_samples(in, dead);

	in->hw.wpos = (in->hw.wpos + samples) % in->hw.samples;
	return samples;
}


static int genode_read(SWVoiceIn *sw, void *buf, int size)
{
	GenodeVoiceIn * const in = (GenodeVoiceIn*)sw->hw;
	Pcm_in_buffer &pcm_buf   = in->pcm_buf;

	size_t const avail = pcm_buf.read_avail();
	if (avail < size)
		Genode::error(__func__, ": avail: ", avail, " size: ", size);

	size_t const r = pcm_buf.read(buf, size);
	if (r != size)
		Genode::error(__func__, ": r: ", r, " size: ", size);

	/* needed by audio_pcm_hw_get_live_in() to calculate ``live'' samples */
	sw->total_hw_samples_acquired += (r / (VBOX_SAMPLE_SIZE*VBOX_CHANNELS));
	return size;
}


static int genode_ctl_in(HWVoiceIn *hw, int cmd, ...)
{
	GenodeVoiceIn * const in = (GenodeVoiceIn*)hw;
	switch (cmd) {
	case VOICE_ENABLE:
		in->packets = 0;
		in->audio->start();
		break;
	case VOICE_DISABLE:
		in->audio->stop();
		break;
	}

	return 0;
}


static void *genode_audio_init(void) { return &oss_audio_driver; }


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
