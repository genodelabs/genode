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
#include <audio_out_session/audio_out_session.h>
#include <audio_in_session/audio_in_session.h>
#include <os/config.h>
#include <os/server.h>
#include <util/xml_node.h>

/* local includes */
#include <audio/audio.h>
#include <bsd.h>

#include <extern_c_begin.h>
# include <bsd_emul.h>
# include <sys/device.h>
# include <sys/audioio.h>
#include <extern_c_end.h>


static bool verbose = false;

extern struct cfdriver audio_cd;

static dev_t const adev = 0x80; /* audio0 (minor nr 128) */
static dev_t const mdev = 0x10; /* mixer0 (minor nr  16) */

static bool adev_usuable = false;


static bool drv_loaded()
{
	/*
	 * The condition is true when at least one PCI device where a
	 * driver is available for was found by the autoconf mechanisum
	 * (see bsd_emul.c).
	 */
	return audio_cd.cd_ndevs > 0 ? true : false;
}


/******************************
 ** Dump audio configuration **
 ******************************/

#define DUMP_INFO(field) \
	PLOG("--- " #field " information ---");        \
	PLOG("sample_rate: %u", ai.field.sample_rate); \
	PLOG("channels: %u",    ai.field.channels);    \
	PLOG("precision: %u",   ai.field.precision);   \
	PLOG("bps: %u",         ai.field.bps);         \
	PLOG("encoding: %u",    ai.field.encoding);    \
	PLOG("gain: %u",        ai.field.gain);        \
	PLOG("port: %u",        ai.field.port);        \
	PLOG("seek: %u",        ai.field.seek);        \
	PLOG("avail_ports: %u", ai.field.avail_ports); \
	PLOG("buffer_size: %u", ai.field.buffer_size); \
	PLOG("block_size: %u",  ai.field.block_size);  \
	PLOG("samples: %u",     ai.field.samples);     \
	PLOG("eof: %u",         ai.field.eof);         \
	PLOG("pause: %u",       ai.field.pause);       \
	PLOG("error: %u",       ai.field.error);       \
	PLOG("waiting: %u",     ai.field.waiting);     \
	PLOG("balance: %u",     ai.field.balance);     \
	PLOG("open: %u",        ai.field.open);        \
	PLOG("active: %u",      ai.field.active)


static void dump_pinfo()
{
	struct audio_info ai;
	if (audioioctl(adev, AUDIO_GETINFO, (char*)&ai, 0, 0)) {
		PERR("could not gather play information");
		return;
	}

	DUMP_INFO(play);
}


static void dump_rinfo()
{
	struct audio_info ai;
	if (audioioctl(adev, AUDIO_GETINFO, (char*)&ai, 0, 0)) {
		PERR("could not gather play information");
		return;
	}

	DUMP_INFO(record);
}


/*************************
 ** Mixer configuration **
 *************************/

struct Mixer
{
	mixer_devinfo_t *info;
	unsigned         num;
};


static Mixer mixer;


static unsigned count_mixer()
{
	mixer_devinfo_t info;
	for (int i = 0; ; i++) {
		info.index = i;
		if (audioioctl(mdev, AUDIO_MIXER_DEVINFO, (char*)&info, 0, 0))
			return i;
	}

	return 0;
}


static mixer_devinfo_t *alloc_mixer(unsigned num)
{
	return (mixer_devinfo_t*)
		mallocarray(num, sizeof(mixer_devinfo_t), 0, M_ZERO);
}


static bool query_mixer(Mixer &mixer)
{
	for (unsigned i = 0; i < mixer.num; i++) {
		mixer.info[i].index = i;
		if (audioioctl(mdev, AUDIO_MIXER_DEVINFO, (char*)&mixer.info[i], 0, 0))
			return false;
	}

	return true;
}


static inline int level(char const *value)
{
	unsigned long res = 0;
	Genode::ascii_to(value, res);
	return res > AUDIO_MAX_GAIN ? AUDIO_MAX_GAIN : res;
}


static bool set_mixer_value(Mixer &mixer, char const * const field,
                                          char const * const value)
{
	char buffer[64];

	for (unsigned i = 0; i < mixer.num; i++) {
		mixer_devinfo_t &info = mixer.info[i];

		if (info.type == AUDIO_MIXER_CLASS)
			continue;

		unsigned mixer_class          = info.mixer_class;
		char const * const class_name = mixer.info[mixer_class].label.name;
		char const * const name       = info.label.name;

		Genode::snprintf(buffer, sizeof(buffer), "%s.%s", class_name, name);
		if (Genode::strcmp(field, buffer) != 0)
			continue;

		mixer_ctrl_t ctrl;
		ctrl.dev                   = info.index;
		ctrl.type                  = info.type;
		ctrl.un.value.num_channels = 2;

		if (audioioctl(mdev, AUDIO_MIXER_READ, (char*)&ctrl, 0, 0)) {
			ctrl.un.value.num_channels = 1;
			if (audioioctl(mdev, AUDIO_MIXER_READ, (char*)&ctrl, 0, 0)) {
				PERR("could not read mixer %d'", ctrl.dev);
				return 0;
			}
		}

		int oldv = -1;
		int newv =  0;
		switch (ctrl.type) {
		case AUDIO_MIXER_ENUM:
			{
				for (int i = 0; i < info.un.e.num_mem; i++)
					if (Genode::strcmp(value, info.un.e.member[i].label.name) == 0) {
						oldv = ctrl.un.ord;
						newv = info.un.e.member[i].ord;

						ctrl.un.ord = newv;
						break;
					}
				break;
			}
		case AUDIO_MIXER_SET:
			{
				for (int i = 0; i < info.un.s.num_mem; i++) {
					if (Genode::strcmp(value, info.un.e.member[i].label.name) == 0) {
						oldv= ctrl.un.mask;
						newv |= info.un.s.member[i].mask;

						ctrl.un.mask = newv;
						break;
					}
				}
				break;
			}
		case AUDIO_MIXER_VALUE:
			{
				oldv = ctrl.un.value.level[0];
				newv = level(value);
				ctrl.un.value.level[0] = newv;

				if (ctrl.un.value.num_channels == 2)
					ctrl.un.value.level[1] = newv;

				break;
			}
		}

		if (oldv == -1)
			break;

		if (audioioctl(mdev, AUDIO_MIXER_WRITE, (char*)&ctrl, FWRITE, 0)) {
			PERR("could not set '%s' from %d to %d", field, oldv, newv);
			break;
		}

		PLOG("%s: %d -> %d", field, oldv, newv);
		return true;
	}

	return false;
}


static char const *get_mixer_value(mixer_devinfo_t *info)
{
	static char buffer[128];

	mixer_ctrl_t ctrl;
	ctrl.dev                   = info->index;
	ctrl.type                  = info->type;
	ctrl.un.value.num_channels = 2;

	if (audioioctl(mdev, AUDIO_MIXER_READ, (char*)&ctrl, 0, 0)) {
		ctrl.un.value.num_channels = 1;
		if (audioioctl(mdev, AUDIO_MIXER_READ, (char*)&ctrl, 0, 0)) {
			PERR("could not read mixer %d'", ctrl.dev);
			return 0;
		}
	}

	switch (ctrl.type) {
	case AUDIO_MIXER_ENUM:
		{
			for (int i = 0; i < info->un.e.num_mem; i++)
				if (ctrl.un.ord == info->un.e.member[i].ord) {
					Genode::snprintf(buffer, sizeof(buffer),
					                 "%s", info->un.e.member[i].label.name);
					break;
				}
			break;
		}
	case AUDIO_MIXER_SET:
		{
			char *p = buffer;
			Genode::size_t n = 0;
			for (int i = 0; i < info->un.s.num_mem; i++)
				if (ctrl.un.mask & info->un.s.member[i].mask)
					n += Genode::snprintf(p + n, sizeof(buffer) - n,
					                      "%s%s", n ? "," : "",
					                      info->un.s.member[i].label.name);
			break;
		}
	case AUDIO_MIXER_VALUE:
		{
			if (ctrl.un.value.num_channels == 2)
				Genode::snprintf(buffer, sizeof(buffer), "%d,%d",
				                 ctrl.un.value.level[0],
				                 ctrl.un.value.level[1]);
			else
				Genode::snprintf(buffer, sizeof(buffer), "%d",
				                 ctrl.un.value.level[0]);
			break;
		}
	}

	return buffer;
}


static void dump_mixer(Mixer const &mixer)
{
	PLOG("--- mixer information ---");
	for (unsigned i = 0; i < mixer.num; i++) {
		if (mixer.info[i].type == AUDIO_MIXER_CLASS)
			continue;

		unsigned mixer_class          = mixer.info[i].mixer_class;
		char const * const class_name = mixer.info[mixer_class].label.name;
		char const * const name       = mixer.info[i].label.name;
		char const * const value      = get_mixer_value(&mixer.info[i]);

		if (value)
			PLOG("%s.%s=%s", class_name, name, value);
	}
}


/******************
 ** Audio device **
 ******************/

static bool open_audio_device(dev_t dev)
{
	if (!drv_loaded())
		return false;

	int err = audioopen(dev, FWRITE|FREAD, 0 /* ifmt */, 0 /* proc */);
	if (err)
		return false;

	return true;
}


static bool config_verbose()
{
	using namespace Genode;

	try {
		return config()->xml_node().attribute("verbose").has_value("yes");
	} catch (...) { }

	return false;
}


static void parse_config(Mixer &mixer)
{
	using namespace Genode;

	PLOG("--- parse config ---");

	Xml_node config_node = config()->xml_node();
	config_node.for_each_sub_node("mixer", [&] (Xml_node node) {
		char field[32];
		char value[16];
		try {
			node.attribute("field").value(field, sizeof(field));
			node.attribute("value").value(value, sizeof(value));

			set_mixer_value(mixer, field, value);
		} catch (Xml_attribute::Nonexistent_attribute) { }
	});
}


static bool configure_audio_device(dev_t dev)
{
	struct audio_info ai;

	int err = audioioctl(adev, AUDIO_GETINFO, (char*)&ai, 0, 0);
	if (err)
		return false;

	using namespace Audio;

	/* configure the device according to our Audio_out session settings */
	ai.play.sample_rate = Audio_out::SAMPLE_RATE;
	ai.play.channels    = Audio_out::MAX_CHANNELS;
	ai.play.encoding    = AUDIO_ENCODING_SLINEAR_LE;
	ai.play.block_size  = Audio_out::MAX_CHANNELS * sizeof(short) * Audio_out::PERIOD;

	/* Configure the device according to our Audio_in session settings
	 *
	 * We use Audio_out::MAX_CHANNELS here because the backend provides us
	 * with two channels that we will mix to one in the front end for now.
	 */
	ai.record.sample_rate = Audio_in::SAMPLE_RATE;
	ai.record.channels    = Audio_out::MAX_CHANNELS;
	ai.record.encoding    = AUDIO_ENCODING_SLINEAR_LE;
	ai.record.block_size  = Audio_out::MAX_CHANNELS * sizeof(short) * Audio_in::PERIOD;

	err = audioioctl(adev, AUDIO_SETINFO, (char*)&ai, 0, 0);
	if (err)
		return false;

	int fullduplex = 1;
	err = audioioctl(adev, AUDIO_SETFD, (char*)&fullduplex, 0, 0);
	if (err)
		return false;

	/* query mixer information */
	mixer.num  = count_mixer();
	mixer.info = alloc_mixer(mixer.num);
	if (!mixer.info || !query_mixer(mixer))
		return false;

	if (config_verbose()) verbose = true;

	if (verbose) dump_pinfo();
	if (verbose) dump_rinfo();
	if (verbose) dump_mixer(mixer);

	parse_config(mixer);

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


/***************************
 ** Notification handling **
 ***************************/

static Genode::Signal_context_capability _play_sigh;
static Genode::Signal_context_capability _record_sigh;


/*
 * These functions are directly called by the audio
 * backend in case an play/record interrupt occures
 * and are used to notify our driver frontend.
 */

extern "C" void notify_play()
{
	if (_play_sigh.valid())
		Genode::Signal_transmitter(_play_sigh).submit();
}


extern "C" void notify_record()
{
	if (_record_sigh.valid())
		Genode::Signal_transmitter(_record_sigh).submit();
}


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
	Bsd::scheduler().schedule();
}


bool Audio::driver_active() { return drv_loaded() && adev_usuable; }


void Audio::play_sigh(Genode::Signal_context_capability sigh) {
	_play_sigh = sigh; }


void Audio::record_sigh(Genode::Signal_context_capability sigh) {
	_record_sigh = sigh; }


int Audio::play(short *data, Genode::size_t size)
{
	struct uio uio = { 0, size, UIO_READ, data, size };
	return audiowrite(adev, &uio, IO_NDELAY);
}


int Audio::record(short *data, Genode::size_t size)
{
	struct uio uio = { 0, size, UIO_WRITE, data, size };
	return audioread(adev, &uio, IO_NDELAY);
}
