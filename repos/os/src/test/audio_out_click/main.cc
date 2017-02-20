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
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <audio_out_session/connection.h>
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/heap.h>
#include <base/log.h>
#include <dataspace/client.h>
#include <input_session/connection.h>
#include <input/event.h>

using Filename = Genode::String<64>;
using namespace Genode;
using namespace Audio_out;

static constexpr bool const verbose = false;
static constexpr char const * channel_names[2] = { "front left", "front right" };


class Click
{
	private:

		enum {
			CHANNELS     = 2,                      /* number of channels */
			FRAME_SIZE   = sizeof(float),
			PERIOD_CSIZE = FRAME_SIZE * PERIOD,    /* size of channel packet (bytes) */
			PERIOD_FSIZE = CHANNELS * PERIOD_CSIZE, /* size of period in file (bytes) */
		};

		Env & _env;
		Constructible<Audio_out::Connection> _audio_out[CHANNELS];

		Filename const & _name;

		Attached_rom_dataspace _sample_ds { _env, _name.string() };
		char     const * const _base = _sample_ds.local_addr<char const>();
		size_t           const _size = _sample_ds.size();

	public:

		Click(Env & env, Filename const & name)
		: _env(env), _name(name)
		{
			for (int i = 0; i < CHANNELS; ++i) {
				/* allocation signal for first channel only */
				_audio_out[i].construct(env, channel_names[i], i == 0);
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


struct Main
{
	Env &                env;
	Signal_handler<Main> handler { env.ep(), *this, &Main::handle };
	Input::Connection    input   { env };
	Input::Event *       ev_buf =
		static_cast<Input::Event*>(env.rm().attach(input.dataspace()));
	Filename const       name { "click.raw" };
	Click                click { env, name };

	void handle()
	{
		for (int i = 0, num_ev = input.flush(); i < num_ev; ++i) {
			Input::Event &ev = ev_buf[i];
			if (ev.type() == Input::Event::PRESS) {
				click.play();
				break;
			}
		}
	}

	Main(Env & env) : env(env)
	{
		log("--- Audio_out click test ---");

		input.sigh(handler);
	}
};


void Component::construct(Env & env) { static Main main(env); }
