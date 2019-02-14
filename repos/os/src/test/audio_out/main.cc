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
#include <rom_session/connection.h>

using Filename = Genode::String<64>;
using namespace Genode;
using namespace Audio_out;

static constexpr bool const verbose = false;
static constexpr char const * channel_names[2] = { "front left", "front right" };


class Track : public Thread
{
	private:

		/*
		 * Noncopyable
		 */
		Track(Track const &);
		Track &operator = (Track const &);

		enum {
			CHN_CNT      = 2,                      /* number of channels */
			FRAME_SIZE   = sizeof(float),
			PERIOD_CSIZE = FRAME_SIZE * PERIOD,    /* size of channel packet (bytes) */
			PERIOD_FSIZE = CHN_CNT * PERIOD_CSIZE, /* size of period in file (bytes) */
		};

		Env & _env;
		Constructible<Audio_out::Connection> _audio_out[CHN_CNT];

		Filename const & _name;

		Attached_rom_dataspace _sample_ds { _env, _name.string() };
		char     const * const _base = _sample_ds.local_addr<char const>();
		size_t           const _size = _sample_ds.size();

	public:

		Track(Env & env, Filename const & name)
		: Thread(env, "track", sizeof(size_t)*2048), _env(env), _name(name)
		{
			/* allocation signal for first channel only */
			for (int i = 0; i < CHN_CNT; ++i)
				_audio_out[i].construct(env, channel_names[i], i == 0);

			start();
		}

		void entry() override
		{
			if (verbose)
				log(_name, " size is ", _size, " bytes "
				    "(attached to ", (void *)_base, ")");

			for (int i = 0; i < CHN_CNT; ++i)
				_audio_out[i]->start();

			unsigned cnt = 0;
			while (1) {

				for (size_t offset = 0, cnt = 1;
				     offset < _size;
				     offset += PERIOD_FSIZE, ++cnt) {

					/*
					 * The current chunk (in number of frames of one channel)
					 * is the size of the period except at the end of the
					 * file.
					 */
					size_t chunk = (offset + PERIOD_FSIZE > _size)
					               ? (_size - offset) / CHN_CNT / FRAME_SIZE
					               : PERIOD;

					Packet *p[CHN_CNT];
					while (1)
						try {
							p[0] = _audio_out[0]->stream()->alloc();
							break;
						} catch (Audio_out::Stream::Alloc_failed) {
							_audio_out[0]->wait_for_alloc();
						}

					unsigned pos = _audio_out[0]->stream()->packet_position(p[0]);
					/* sync other channels with first one */
					for (int chn = 1; chn < CHN_CNT; ++chn)
						p[chn] = _audio_out[chn]->stream()->get(pos);

					/* copy channel contents into sessions */
					float *content = (float *)(_base + offset);
					for (unsigned c = 0; c < CHN_CNT * chunk; c += CHN_CNT)
						for (int i = 0; i < CHN_CNT; ++i)
							p[i]->content()[c / 2] = content[c + i];

					/* handle last packet gracefully */
					if (chunk < PERIOD) {
						for (int i = 0; i < CHN_CNT; ++i)
							memset(p[i]->content() + chunk,
							       0, PERIOD_CSIZE - FRAME_SIZE * chunk);
					}

					if (verbose)
						log(_name, " submit packet ",
						    _audio_out[0]->stream()->packet_position((p[0])));

					for (int i = 0; i < CHN_CNT; i++)
						_audio_out[i]->submit(p[i]);
				}

				log("played '", _name, "' ", ++cnt, " time(s)");
			}
		}
};


struct Main
{
	enum { MAX_FILES = 16 };

	Env &                  env;
	Heap                   heap { env.ram(), env.rm() };
	Attached_rom_dataspace config { env, "config" };
	Filename               filenames[MAX_FILES];
	unsigned               track_count = 0;

	void handle_config();

	Main(Env & env);
};


void Main::handle_config()
{
	try {
		config.xml().for_each_sub_node("filename",
		                               [this] (Xml_node const & node)
		{
			if (!(track_count < MAX_FILES)) {
				warning("test supports max ", (int)MAX_FILES,
				        " files. Skipping...");
				return;
			}

			node.with_raw_content([&] (char const *start, size_t length) {
				filenames[track_count++] = Filename(Cstring(start, length)); });
		});
	}
	catch (...) {
		warning("couldn't get input files, failing back to defaults");
		filenames[0] = Filename("1.raw");
		filenames[1] = Filename("2.raw");
		track_count  = 2;
	}
}


Main::Main(Env & env) : env(env)
{
	log("--- Audio_out test ---");

	handle_config();

	for (unsigned i = 0; i < track_count; ++i)
		new (heap) Track(env, filenames[i]);
}


void Component::construct(Env & env) { static Main main(env); }
