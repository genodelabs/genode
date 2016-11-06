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
#include <rom_session/connection.h>
#include <dataspace/client.h>
#include <os/config.h>


using namespace Genode;


using namespace Genode;
using namespace Audio_out;

static const bool verbose = false;

enum {
	CHN_CNT      = 2,                      /* number of channels */
	FRAME_SIZE   = sizeof(float),
	PERIOD_CSIZE = FRAME_SIZE * PERIOD,    /* size of channel packet (bytes) */
	PERIOD_FSIZE = CHN_CNT * PERIOD_CSIZE, /* size of period in file (bytes) */
};


static const char *channel_names[] = { "front left", "front right" };


class Track : Thread_deprecated<8192>
{
	private:

		Lazy_volatile_object<Audio_out::Connection> _audio_out[CHN_CNT];

		String<64> const _name;

		Attached_rom_dataspace _sample_ds { _name.string() };
		char     const * const _base = _sample_ds.local_addr<char const>();
		size_t           const _size = _sample_ds.size();

	public:

		Track(const char *name) : Thread_deprecated("track"), _name(name)
		{
			for (int i = 0; i < CHN_CNT; ++i) {
				/* allocation signal for first channel only */
				_audio_out[i].construct(channel_names[i], i == 0);
			}
		}

		void entry()
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

		void ready()
		{
			start();
		}
};


static int process_config(const char ***files)
{
	enum { MAX_FILES = 16 };

	static char  file_mem[64][MAX_FILES];
	static const char *file_p[MAX_FILES];
	int cnt = 0;

	Xml_node config_node = config()->xml_node();

	for (unsigned i = 0; i < config_node.num_sub_nodes(); ++i) {

		if (!(i < MAX_FILES)) {
			warning("test supports max ", (int)MAX_FILES, " files. Skipping...");
			break;
		}

		Xml_node file_node = config_node.sub_node(i);

		if (!config_node.has_type("config")) {
			error("root node of config file is not a <config> tag");
			return -1;
		}

		if (file_node.has_type("filename")) {
			memcpy(file_mem[cnt], file_node.content_addr(), file_node.content_size());
			file_p[cnt] = file_mem[cnt];
			file_mem[cnt][file_node.content_size()] = '\0';
			cnt++;
		}
	}

	*files = file_p;
	return cnt;
}


int main(int argc, char **argv)
{
	log("--- Audio_out test ---");

	const char *defaults[] = { "1.raw", "2.raw" };
	const char **files     = defaults;
	int cnt                = 2;

	try {
		cnt = process_config(&files);
	}
	catch (...) {
		warning("couldn't get input files, failing back to defaults");
	}

	Track *track[cnt];

	for (int i = 0; i < cnt; ++i) {
		track[i] = new (env()->heap()) Track(files[i]);
	}

	/* start playback after constrution of all tracks */
	for (int i = 0; i < cnt; i++)
		track[i]->ready();

	sleep_forever();
	return 0;
}


