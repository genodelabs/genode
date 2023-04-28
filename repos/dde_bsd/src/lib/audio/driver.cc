/*
 * \brief  Audio driver BSD API emulation
 * \author Josef Soentgen
 * \date   2014-11-09
 */

/*
 * Copyright (C) 2014-2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <audio_out_session/audio_out_session.h>
#include <audio_in_session/audio_in_session.h>
#include <base/env.h>
#include <base/log.h>
#include <os/reporter.h>
#include <util/xml_node.h>

/* format-string includes */
#include <format/snprintf.h>

/* local includes */
#include <audio/audio.h>
#include <bsd.h>
#include <bsd_emul.h>
#include <scheduler.h>

#include <extern_c_begin.h>
# include <sys/device.h>
# include <sys/audioio.h>
#include <extern_c_end.h>


extern struct cfdriver audio_cd;

static dev_t const adev = 0x00; /* /dev/audio0 */
static dev_t const mdev = 0xc0; /* /dev/audioctl */

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

static void dump_info()
{
	struct audio_swpar ap;

	AUDIO_INITPAR(&ap);

	if (audioioctl(adev, AUDIO_GETPAR, (char*)&ap, 0, 0)) {
		Genode::error("could not gather play information");
		return;
	}

	Genode::log("Audio information:");
	Genode::log("  sample_rate:       ", (unsigned)ap.rate);
	Genode::log("  playback channels: ", (unsigned)ap.pchan);
	Genode::log("  record channels:   ", (unsigned)ap.rchan);
	Genode::log("  num blocks:        ", (unsigned)ap.nblks);
	Genode::log("  block size:        ", (unsigned)ap.round);
}


/*************************
 ** Mixer configuration **
 *************************/

struct Mixer
{
	mixer_devinfo_t *info;
	unsigned         num;

	bool report_state;
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

		Format::snprintf(buffer, sizeof(buffer), "%s.%s", class_name, name);
		if (Genode::strcmp(field, buffer) != 0)
			continue;

		mixer_ctrl_t ctrl;
		ctrl.dev                   = info.index;
		ctrl.type                  = info.type;
		ctrl.un.value.num_channels = 2;

		if (audioioctl(mdev, AUDIO_MIXER_READ, (char*)&ctrl, 0, 0)) {
			ctrl.un.value.num_channels = 1;
			if (audioioctl(mdev, AUDIO_MIXER_READ, (char*)&ctrl, 0, 0)) {
				Genode::error("could not read mixer ", ctrl.dev);
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
					if (Genode::strcmp(value, info.un.s.member[i].label.name) == 0) {
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
			Genode::error("could not set ", field, " from ", oldv, " to ", newv);
			break;
		}

		return true;
	}

	return false;
}


static char const *get_mixer_value(mixer_devinfo_t const *info)
{
	static char buffer[128];

	mixer_ctrl_t ctrl;
	ctrl.dev                   = info->index;
	ctrl.type                  = info->type;
	ctrl.un.value.num_channels = 2;

	if (audioioctl(mdev, AUDIO_MIXER_READ, (char*)&ctrl, 0, 0)) {
		ctrl.un.value.num_channels = 1;
		if (audioioctl(mdev, AUDIO_MIXER_READ, (char*)&ctrl, 0, 0)) {
			Genode::error("could not read mixer ", ctrl.dev);
			return 0;
		}
	}

	switch (ctrl.type) {
	case AUDIO_MIXER_ENUM:
		{
			for (int i = 0; i < info->un.e.num_mem; i++)
				if (ctrl.un.ord == info->un.e.member[i].ord) {
					Format::snprintf(buffer, sizeof(buffer),
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
					n += Format::snprintf(p + n, sizeof(buffer) - n,
					                      "%s%s", n ? "," : "",
					                      info->un.s.member[i].label.name);
			break;
		}
	case AUDIO_MIXER_VALUE:
		{
			if (ctrl.un.value.num_channels == 2)
				Format::snprintf(buffer, sizeof(buffer), "%d,%d",
				                 ctrl.un.value.level[0],
				                 ctrl.un.value.level[1]);
			else
				Format::snprintf(buffer, sizeof(buffer), "%d",
				                 ctrl.un.value.level[0]);
			break;
		}
	}

	return buffer;
}


static bool headphone_plugged()
{
	for (unsigned i = 0; i < mixer.num; i++) {
		mixer_devinfo_t const &info = mixer.info[i];

		if (info.type == AUDIO_MIXER_CLASS) {
			continue;
		}

		unsigned     const mixer_class = info.mixer_class;
		char const * const class_name  = mixer.info[mixer_class].label.name;
		char const * const name        = info.label.name;

		Genode::String<64> const control { class_name, ".", name };
		if (control != "outputs.hp_sense") {
			continue;
		}

		auto const result = get_mixer_value(&info);

		return Genode::strcmp("plugged", result) == 0;
	}

	return false;
}


static void dump_mixer(Mixer const &mixer)
{
	Genode::log("--- mixer information ---");
	for (unsigned i = 0; i < mixer.num; i++) {
		if (mixer.info[i].type == AUDIO_MIXER_CLASS)
			continue;

		unsigned mixer_class          = mixer.info[i].mixer_class;
		char const * const class_name = mixer.info[mixer_class].label.name;
		char const * const name       = mixer.info[i].label.name;
		char const * const value      = get_mixer_value(&mixer.info[i]);

		if (value)
			Genode::log(class_name, ".", name, "=", value);
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

	/* try to open playback only, if capturing potentially failed */
	if (err == ENODEV)
		err = audioopen(dev, FWRITE, 0 /* ifmt */, 0 /* proc */);

	if (err)
		return false;

	return true;
}


static void report_mixer_state(Mixer &mixer, Genode::Env *env = nullptr)
{
	using namespace Genode;

	static Constructible<Expanding_reporter> mixer_reporter { };

	if (!mixer.report_state)
		return;

	if (env && !mixer_reporter.constructed())
		mixer_reporter.construct(*env, "mixer_state", "mixer_state");

	if (!mixer_reporter.constructed())
		return;

	mixer_reporter->generate([&] (Xml_generator &xml) {

		for (unsigned i = 0; i < mixer.num; i++) {
			if (mixer.info[i].type == AUDIO_MIXER_CLASS)
				continue;

			unsigned mixer_class          = mixer.info[i].mixer_class;
			char const * const class_name = mixer.info[mixer_class].label.name;
			char const * const name       = mixer.info[i].label.name;
			char const * const value      = get_mixer_value(&mixer.info[i]);

			if (value) {
				xml.node("mixer", [&]() {
					char tmp[64];
					Format::snprintf(tmp, sizeof(tmp), "%s.%s",
					                 class_name, name);

					xml.attribute("field", tmp);
					xml.attribute("value", value);
				});
			}
		}
	});
}


static void configure_mixer(Genode::Env &env, Mixer &mixer, Genode::Xml_node config)
{
	using namespace Genode;

	config.for_each_sub_node("mixer", [&] (Xml_node node) {

		typedef String<32> Field;
		typedef String<16> Value;

		Field const field = node.attribute_value("field", Field());
		Value const value = node.attribute_value("value", Value());

		set_mixer_value(mixer, field.string(), value.string());
	});

	mixer.report_state = config.attribute_value<bool>("report_mixer", false);
	report_mixer_state(mixer, &env);
}


static bool configure_audio_device(Genode::Env &env, dev_t dev, Genode::Xml_node config)
{
	struct audio_swpar ap;

	AUDIO_INITPAR(&ap);

	int err = audioioctl(adev, AUDIO_GETPAR, (char*)&ap, 0, 0);
	if (err)
		return false;

	using namespace Audio;

	/*
	 * Configure the device according to our Audio_out session parameters.
	 * Only set the relevant parameters and let the audio(4) subsystem
	 * figure out the rest.
	 */
	ap.rate  = Audio_out::SAMPLE_RATE;
	ap.pchan = Audio_out::MAX_CHANNELS;
	ap.sig   = 1;
	ap.bits  = 16;
	ap.bps   = (ap.bits / 8);
	ap.round = Audio_out::PERIOD;
	/*
	 * Use 2 blocks, the one that is currently played and the one
	 * that will be filled in.
	 */
	ap.nblks = 2;
	/*
	 * For recording use two channels that we will mix to one in the
	 * front end.
	 */
	ap.rchan = 2;

	err = audioioctl(adev, AUDIO_SETPAR, (char*)&ap, 0, 0);
	if (err)
		return false;

	/* query mixer information */
	mixer.num  = count_mixer();
	mixer.info = alloc_mixer(mixer.num);
	if (!mixer.info || !query_mixer(mixer))
		return false;

	bool const verbose = config.attribute_value<bool>("verbose", false);

	if (verbose) dump_info();
	if (verbose) dump_mixer(mixer);

	configure_mixer(env, mixer, config);

	notify_hp_sense(headphone_plugged() ? 1: 0);

	return true;
}


static void run_bsd(void *p);


namespace {

	struct Task_args
	{
		Genode::Env       &env;
		Genode::Allocator &alloc;
		Genode::Xml_node   config;
		Genode::Signal_context_capability announce_sigh;

		Task_args(Genode::Env &env, Genode::Allocator &alloc,
		          Genode::Xml_node config,
		          Genode::Signal_context_capability announce_sigh)
		:
			env(env), alloc(alloc), config(config),
			announce_sigh(announce_sigh)
		{ }
	};

	struct Task
	{
		Task_args _args;

		Bsd::Task _task;

		Genode::Signal_handler<Task> _handler;

		void _handle_signal()
		{
			_task.unblock();
			Bsd::scheduler().schedule();
		}

		struct Operation
		{
			struct uio uio;
			bool       pending;
			int        result;
		};

		Operation _play   { { 0, 0, UIO_READ,  nullptr, 0 }, false, -1 };
		Operation _record { { 0, 0, UIO_WRITE, nullptr, 0 }, false, -1 };

		template <typename... ARGS>
		Task(Genode::Env &env, Genode::Allocator &alloc,
		     Genode::Xml_node config,
		     Genode::Signal_context_capability announce_sigh)
		:
			_args { env, alloc, config, announce_sigh },
			_task { run_bsd, this, "bsd", Bsd::Task::PRIORITY_0,
			        Bsd::scheduler(), 2048 * sizeof(Genode::addr_t) },
			_handler { env.ep(), *this, &Task::_handle_signal }
		{ }

		void unblock() { _task.unblock(); }

		void request_playback(short *src, size_t size)
		{
			_play.uio     = { 0, size, UIO_READ, src, size };
			_play.pending = true;
			_play.result  = -1;
		}

		int playback_result() const
		{
			return _play.result;
		}

		void request_recording(short *dst, size_t size)
		{
			_record.uio     = { 0, size, UIO_WRITE, dst, size };
			_record.pending = true;
			_record.result  = -1;
		}

		int recording_result() const
		{
			return _record.result;
		}
	};
}


void run_bsd(void *p)
{
	Task *task = static_cast<Task*>(p);

	int const success = Bsd::probe_drivers(task->_args.env,
	                                       task->_args.alloc);
	if (!success) {
		Genode::error("no supported sound card found");
		Genode::sleep_forever();
	}

	if (!open_audio_device(adev)) {
		Genode::error("could not initialize sound card");
		Genode::sleep_forever();
	}

	adev_usuable = configure_audio_device(task->_args.env, adev,
	                                      task->_args.config);

	if (adev_usuable && task->_args.announce_sigh.valid()) {
		Genode::Signal_transmitter(task->_args.announce_sigh).submit();
	}

	while (true) {
		Bsd::scheduler().current()->block_and_schedule();

		if (task->_play.pending) {
			task->_play.result = audiowrite(adev, &task->_play.uio, IO_NDELAY);
			task->_play.pending = false;
		}
		if (task->_record.pending) {
			task->_record.result = audioread(adev, &task->_record.uio, IO_NDELAY);
			task->_record.pending = false;
		}
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


extern "C" void notify_hp_sense(int const sense)
{
	set_mixer_value(mixer, "record.adc-0:1_source", sense ? "mic2" : "mic");

	report_mixer_state(mixer, nullptr);
}


/*****************************
 ** private Audio namespace **
 *****************************/

void Audio::update_config(Genode::Env &env, Genode::Xml_node config)
{
	if (mixer.info == nullptr) { return; }

	configure_mixer(env, mixer, config);
}


static Task *_bsd_task;


void Audio::init_driver(Genode::Env &env, Genode::Allocator &alloc,
                        Genode::Xml_node config,
                        Genode::Signal_context_capability announce_sigh)
{
	Bsd::mem_init(env, alloc);
	Bsd::timer_init(env);

	static Task bsd_task(env, alloc, config, announce_sigh);
	_bsd_task = &bsd_task;
	Bsd::scheduler().schedule();
}


void Audio::play_sigh(Genode::Signal_context_capability sigh) {
	_play_sigh = sigh; }


void Audio::record_sigh(Genode::Signal_context_capability sigh) {
	_record_sigh = sigh; }


int Audio::play(short *data, Genode::size_t size)
{
	_bsd_task->request_playback(data, size);
	_bsd_task->unblock();
	Bsd::scheduler().schedule();
	return _bsd_task->playback_result();
}


int Audio::record(short *data, Genode::size_t size)
{
	_bsd_task->request_recording(data, size);
	_bsd_task->unblock();
	Bsd::scheduler().schedule();
	return _bsd_task->recording_result();
}
