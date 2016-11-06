/*
 * \brief  Audio-out test implementation
 * \author Sebastian Sumpf
 * \author Christian Helmuth
 * \date   2009-12-03
 *
 * The test program plays several tracks simultaneously to the Audio_out
 * service. See README for the configuration.
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <audio_out_session/connection.h>
#include <base/log.h>
#include <base/sleep.h>
#include <base/attached_rom_dataspace.h>
#include <dataspace/client.h>
#include <input_session/connection.h>
#include <input/event.h>
#include <os/config.h>


using namespace Genode;
using namespace Audio_out;

static const bool verbose = false;

enum {
	CHANNELS     = 2,                      /* number of channels */
	FRAME_SIZE   = sizeof(float),
	PERIOD_CSIZE = FRAME_SIZE * PERIOD,     /* size of channel packet (bytes) */
	PERIOD_FSIZE = CHANNELS * PERIOD_CSIZE, /* size of period in file (bytes) */
};


static const char *channel_names[] = { "front left", "front right" };


class Click
{
	private:

		Lazy_volatile_object<Audio_out::Connection> _audio_out[CHANNELS];

		Attached_rom_dataspace _sample_ds;
		char     const * const _base = _sample_ds.local_addr<char const>();
		size_t           const _size = _sample_ds.size();

	public:

		Click(char const *file) : _sample_ds(file)
		{
			for (int i = 0; i < CHANNELS; ++i) {
				/* allocation signal for first channel only */
				_audio_out[i].construct(channel_names[i], i == 0);
				_audio_out[i]->start();
			}
		}

		void play()
		{
			log("play click");

			for (int i = 0; i < CHANNELS; i++)
				_audio_out[i]->stream()->reset();

			for (Genode::size_t offset = 0; offset < _size; offset += PERIOD_FSIZE) {

				/*
				 * The current chunk (in number of frames of one channel)
				 * is the size of the period except at the end of the
				 * file.
				 */
				size_t chunk = (offset + PERIOD_FSIZE > _size)
				               ? (_size - offset) / CHANNELS / FRAME_SIZE
				               : PERIOD;

				Packet *p[CHANNELS];

				while (true)
					try {
						p[0] = _audio_out[0]->stream()->alloc();
						break;
					} catch (Audio_out::Stream::Alloc_failed) {
						_audio_out[0]->wait_for_alloc();
					}

				unsigned pos = _audio_out[0]->stream()->packet_position(p[0]);
				for (int chn = 1; chn < CHANNELS; ++chn)
					p[chn] = _audio_out[chn]->stream()->get(pos);

				/* copy channel contents into sessions */
				float *content = (float *)(_base + offset);
				for (unsigned c = 0; c < CHANNELS * chunk; c += CHANNELS)
					for (int i = 0; i < CHANNELS; ++i)
						p[i]->content()[c / 2] = content[c + i];

				/* handle last packet gracefully */
				if (chunk < PERIOD) {
					for (int i = 0; i < CHANNELS; ++i)
						memset(p[i]->content() + chunk,
						       0, PERIOD_CSIZE - FRAME_SIZE * chunk);
				}

				for (int i = 0; i < CHANNELS; i++)
					_audio_out[i]->submit(p[i]);
			}
		}
};

int main(int argc, char **argv)
{
	log("--- Audio_out click test ---");

	Genode::Signal_context  sig_ctx;
	Genode::Signal_receiver sig_rec;

	Genode::Signal_context_capability sig_cap = sig_rec.manage(&sig_ctx);

	Input::Connection input;
	Input::Event      *ev_buf;

	input.sigh(sig_cap);
	ev_buf = static_cast<Input::Event *>(Genode::env()->rm_session()->attach(input.dataspace()));

	Click click("click.raw");

	for (;;) {
		Genode::Signal sig = sig_rec.wait_for_signal();

		for (int i = 0, num_ev = input.flush(); i < num_ev; ++i) {
			Input::Event &ev = ev_buf[i];
			if (ev.type() == Input::Event::PRESS) {
				click.play();
				break;
			}
		}
	}

	return 0;
}
