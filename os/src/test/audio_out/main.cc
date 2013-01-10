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

#include <base/allocator_avl.h>
#include <base/printf.h>
#include <base/sleep.h>
#include <base/thread.h>
#include <audio_out_session/connection.h>
#include <os/config.h>

using namespace Genode;
using namespace Audio_out;

static const bool verbose = false;

enum {
	CHN_CNT      = 2,                      /* number of channels */
	PERIOD_CSIZE = FRAME_SIZE * PERIOD,    /* size of channel packet (bytes) */
	PERIOD_FSIZE = CHN_CNT * PERIOD_CSIZE, /* size of period in file (bytes) */

};

static const char *channel_names[] = { "front left", "front right" };


class Track : Thread<8192>
{
	private:

		typedef Audio_out::Session::Channel::Source Stream;

		const char            *_file;
		Audio_out::Connection *_audio_out[CHN_CNT];
		Stream                *_stream[CHN_CNT];

	public:

		Track(const char *file, Allocator_avl *block_alloc[CHN_CNT]) : _file(file)
		{
			for (int i = 0; i < CHN_CNT; ++i) {
				_audio_out[i] = new (env()->heap())
				                Audio_out::Connection(channel_names[i], block_alloc[i]);
				_stream[i]    = _audio_out[i]->stream();
				if (i > 0)
					_audio_out[i]->sync_session(_audio_out[i-1]->session_capability());
			}
		}

		void entry()
		{
			char *base;
			Dataspace_capability ds_cap;

			try {
				Rom_connection rom(_file);
				rom.on_destruction(Rom_connection::KEEP_OPEN);
				ds_cap = rom.dataspace();
				base = env()->rm_session()->attach(ds_cap);
			}
			catch (...) {
				PDBG("Error: Could not open: %s", _file);
				return;
			}

			Dataspace_client ds_client(ds_cap);

			if (verbose)
				PDBG("%s size is %zu Bytes (attached to %p)",
				     _file, ds_client.size(), base);

			Packet_descriptor p[CHN_CNT];
			size_t            file_size       = ds_client.size();

			while (1) {

				for (size_t offset = 0, cnt = 1;
				     offset < file_size;
				     offset += PERIOD_FSIZE, ++cnt) {

					/*
					 * The current chunk (in number of frames of one channel)
					 * is the size of the period except at the end of the
					 * file.
					 */
					size_t chunk = (offset + PERIOD_FSIZE > file_size)
					               ? (file_size - offset) / CHN_CNT / FRAME_SIZE
					               : PERIOD;

					for (int chn = 0; chn < CHN_CNT; ++chn)
						while (1)
							try {
								p[chn] = _stream[chn]->alloc_packet(PERIOD_CSIZE);
								break;
							} catch (Stream::Packet_alloc_failed) {
								/* wait for next finished packet */
								_stream[chn]->release_packet(_stream[chn]->get_acked_packet());
							}

					/* copy channel contents into sessions */
					float *content = (float *)(base + offset);
					for (unsigned c = 0; c < CHN_CNT * chunk; c += CHN_CNT)
						for (int i = 0; i < CHN_CNT; ++i)
							_stream[i]->packet_content(p[i])[c / 2] = content[c + i];

					/* handle last packet gracefully */
					if (chunk < PERIOD) {
						for (int i = 0; i < CHN_CNT; ++i)
							memset((_stream[i]->packet_content(p[i]) + chunk),
							       0, PERIOD_CSIZE - FRAME_SIZE * chunk);
					}

					if (verbose)
						PDBG("%s submit packet %zu", _file, cnt);

					for (int i = 0; i < CHN_CNT; ++i)
						_stream[i]->submit_packet(p[i]);

					for (int i = 0; i < CHN_CNT; ++i)
						while (_stream[i]->ack_avail() ||
						       !_stream[i]->ready_to_submit()) {
							_stream[i]->release_packet(_stream[i]->get_acked_packet());
						}
				}

				/* ack remaining packets */
				for (int i = 0; i < CHN_CNT; ++i)
						while (_stream[i]->ack_avail())
							_stream[i]->release_packet(_stream[i]->get_acked_packet());
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
			PWRN("Test supports max %d files. Skipping...", MAX_FILES);
			break;
		}

		Xml_node file_node = config_node.sub_node(i);

		if (!config_node.has_type("config")) {
			printf("Error: Root node of config file is not a <config> tag.\n");
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
	PDBG("--- Audio-out test ---\n");

	const char *defaults[] = { "1.raw", "2.raw" };
	const char **files     = defaults;
	int cnt                = 2;

	try {
		cnt = process_config(&files);
	}
	catch (...) {
		PWRN("Couldn't get input files, failing back to defaults");
	}

	Track *track[cnt];
	Allocator_avl *block_alloc[cnt][CHN_CNT];
	for (int i = 0; i < cnt; ++i) {
		for (int j = 0; j < CHN_CNT; ++j)
			block_alloc[i][j] = new (env()->heap()) Allocator_avl(env()->heap());
		track[i] = new (env()->heap()) Track(files[i], block_alloc[i]);
	}

	/* start playback after constrution of all tracks */
	for (int i = 0; i < cnt; i++)
		track[i]->ready();

	sleep_forever();
	return 0;
}
