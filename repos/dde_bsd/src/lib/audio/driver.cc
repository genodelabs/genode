/*
 * \brief  Audio driver BSD API emulation
 * \author Josef Soentgen
 * \date   2014-11-09
 */

/*
 * Copyright (C) 2014-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/signal.h> /* FIXME needed by audio_out_session.h */
#include <audio_out_session/audio_out_session.h>
#include <os/server.h>

/* local includes */
#include <audio/audio.h>
#include <bsd.h>

#include <extern_c_begin.h>
# include <bsd_emul.h>
# include <sys/device.h>
# include <sys/audioio.h>
#include <extern_c_end.h>


static bool const verbose = false;

extern struct cfdriver audio_cd;

static dev_t const adev = 0x80; /* audio0 128 */
static dev_t const mdev = 0x10; /* mixer0 16 */

static bool adev_usuable = false;


static bool drv_loaded() { return audio_cd.cd_ndevs > 0 ? true : false; }


static void dump_prinfo(struct audio_prinfo *prinfo)
{
	struct audio_info ai;

	int err = audioioctl(adev, AUDIO_GETINFO, (char*)&ai, 0, 0);
	if (err) {
		PERR("could not gather play information");
		return;
	}

	PLOG("--- play information ---");
	PLOG("sample_rate: %u", prinfo->sample_rate);
	PLOG("channels: %u", prinfo->channels);
	PLOG("precision: %u", prinfo->precision);
	PLOG("bps: %u", prinfo->bps);
	PLOG("encoding: %u", prinfo->encoding);
	PLOG("gain: %u", prinfo->gain);
	PLOG("port: %u", prinfo->port);
	PLOG("seek: %u", prinfo->seek);
	PLOG("avail_ports: %u", prinfo->avail_ports);
	PLOG("buffer_size: %u", prinfo->buffer_size);
	PLOG("block_size: %u", prinfo->block_size);
	/* current state of the device */
	PLOG("samples: %u", prinfo->samples);
	PLOG("eof: %u", prinfo->eof);
	PLOG("pause: %u", prinfo->pause);
	PLOG("error: %u", prinfo->error);
	PLOG("waiting: %u", prinfo->waiting);
	PLOG("balance: %u", prinfo->balance);
	PLOG("open: %u", prinfo->open);
	PLOG("active: %u", prinfo->active);
}


static bool open_audio_device(dev_t dev)
{
	if (!drv_loaded())
		return false;

	int err = audioopen(dev, FWRITE, 0 /* ifmt */, 0 /* proc */);
	if (err)
		return false;

	return true;
}


static bool configure_audio_device(dev_t dev)
{
	struct audio_info ai;

	int err = audioioctl(adev, AUDIO_GETINFO, (char*)&ai, 0, 0);
	if (err)
		return false;

	using namespace Audio;

	/* configure the device according to our Audio_session settings */
	ai.play.sample_rate = Audio_out::SAMPLE_RATE;
	ai.play.channels    = MAX_CHANNELS;
	ai.play.encoding    = AUDIO_ENCODING_SLINEAR_LE;
	ai.play.block_size  = MAX_CHANNELS * sizeof(short) * Audio_out::PERIOD;

	err = audioioctl(adev, AUDIO_SETINFO, (char*)&ai, 0, 0);
	if (err)
		return false;

	if (verbose)
		dump_prinfo(&ai.play);

	return true;
}


static void run_bsd(void *)
{
	if (!Bsd::probe_drivers()) {
		PERR("no supported sound card found");
		Genode::sleep_forever();
	}

	if (!open_audio_device(adev)) {
		PERR("could not initialize sound card");
		Genode::sleep_forever();
	}

	adev_usuable = configure_audio_device(adev);

	while (true) {
		Bsd::scheduler().current()->block_and_schedule();
	}
}

static Bsd::Task *_task;


/*****************************
 ** private Audio namespace **
 *****************************/

void Audio::init_driver(Server::Entrypoint &ep)
{
	Bsd::irq_init(ep);
	Bsd::timer_init(ep);

	static Bsd::Task task_bsd(run_bsd, nullptr, "bsd",
	                          Bsd::Task::PRIORITY_0, Bsd::scheduler(),
	                          2048 * sizeof(long));

	_task = &task_bsd;

	Bsd::scheduler().schedule();
}


bool Audio::driver_active()
{
	return drv_loaded() && adev_usuable;
}


int Audio::play(short *data, Genode::size_t size)
{
	struct uio uio = { 0, size, data, size };
	return audiowrite(adev, &uio, IO_NDELAY);
}
