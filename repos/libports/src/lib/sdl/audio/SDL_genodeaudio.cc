/*
 * \brief  Genode-specific audio backend
 * \author Christian Prochaska
 * \author Sebastian Sumpf
 * \date   2012-03-13
 *
 * based on the dummy SDL audio driver
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/allocator_avl.h>
#include <base/attached_rom_dataspace.h>
#include <base/log.h>
#include <base/thread.h>
#include <audio_out_session/connection.h>
#include <util/reconstructible.h>

/* local includes */
#include <SDL_genode_internal.h>


extern Genode::Env  *global_env();
extern Genode::Lock  event_lock;


enum {
	AUDIO_CHANNELS = 2,
};

using Genode::Allocator_avl;
using Genode::Signal_context;
using Genode::Signal_receiver;
using Genode::log;
using Genode::Hex;
using Genode::Constructible;

static const char *channel_names[] = { "front left", "front right" };
static Signal_context config_signal_context;

extern "C" {

#include "SDL_config.h"

/* Output audio to Genode audio service. */

#include "SDL_rwops.h"
#include "SDL_timer.h"
#include "SDL_audio.h"
#include "SDL_audiomem.h"
#include "SDL_audio_c.h"
#include "SDL_audiodev_c.h"
#include "SDL_genodeaudio.h"

/* The tag name used by Genode audio */
#define GENODEAUD_DRIVER_NAME "genode"

struct Volume_config
{
	Genode::Env        &_env;

	Genode::Attached_rom_dataspace _config_rom { _env, "config" };

	float volume { 1.0f };

	void _handle_config_update()
	{
		_config_rom.update();

		if (!_config_rom.valid()) { return; }

		Genode::Lock_guard<Genode::Lock> guard(event_lock);

		Genode::Xml_node config = _config_rom.xml();

		try {
			unsigned int config_volume;
			config.sub_node("sdl_audio_volume").attribute("value")
				.value(&config_volume);
			volume = (float)config_volume / 100;
		} catch (...) { }

		Genode::log("Change SDL audio volume to ", volume * 100);
	}

	Genode::Signal_handler<Volume_config> _config_handler {
		_env.ep(), *this, &Volume_config::_handle_config_update };

	Volume_config(Genode::Env &env) : _env(env)
	{
		_config_rom.sigh(_config_handler);
		_handle_config_update();
	}
};



struct SDL_PrivateAudioData {
	Uint8             *mixbuf;
	Uint32             mixlen;
	Constructible<Volume_config> volume_config;
	Constructible<Audio_out::Connection> audio[AUDIO_CHANNELS];
	Audio_out::Packet     *packet[AUDIO_CHANNELS];
};


/* Audio driver functions */
static int GENODEAUD_OpenAudio(_THIS, SDL_AudioSpec *spec);
static void GENODEAUD_WaitAudio(_THIS);
static void GENODEAUD_PlayAudio(_THIS);
static Uint8 *GENODEAUD_GetAudioBuf(_THIS);
static void GENODEAUD_CloseAudio(_THIS);


/* Audio driver bootstrap functions */
static int GENODEAUD_Available(void)
{
	return 1;
}


static void GENODEAUD_DeleteDevice(SDL_AudioDevice *device)
{
	for (int channel = 0; channel < AUDIO_CHANNELS; channel++)
		device->hidden->audio[channel].destruct();

	SDL_free(device->hidden);
	SDL_free(device);
}


static SDL_AudioDevice *GENODEAUD_CreateDevice(int devindex)
{
	SDL_AudioDevice *_this;

	/* Initialize all variables that we clean on shutdown */
	_this = (SDL_AudioDevice *)SDL_malloc(sizeof(SDL_AudioDevice));
	if ( _this ) {
		SDL_memset(_this, 0, (sizeof *_this));
		_this->hidden = (struct SDL_PrivateAudioData *)
				SDL_malloc((sizeof *_this->hidden));
	}
	if ( (_this == NULL) || (_this->hidden == NULL) ) {
		SDL_OutOfMemory();
		if ( _this ) {
			SDL_free(_this);
		}
		return(0);
	}
	SDL_memset(_this->hidden, 0, (sizeof *_this->hidden));

	/* Set the function pointers */
	_this->OpenAudio = GENODEAUD_OpenAudio;
	_this->WaitAudio = GENODEAUD_WaitAudio;
	_this->PlayAudio = GENODEAUD_PlayAudio;
	_this->GetAudioBuf = GENODEAUD_GetAudioBuf;
	_this->CloseAudio = GENODEAUD_CloseAudio;

	_this->free = GENODEAUD_DeleteDevice;

	/* connect to 'Audio_out' service */
	for (int channel = 0; channel < AUDIO_CHANNELS; channel++) {
		try {
			_this->hidden->audio[channel].construct(*global_env(),
				channel_names[channel], false, channel == 0 ? true : false);
			_this->hidden->audio[channel]->start();
		}
		catch(Genode::Service_denied) {
			Genode::error("could not connect to 'Audio_out' service");

			while(--channel > 0)
				_this->hidden->audio[channel].destruct();

			return NULL;
		}
	}

	_this->hidden->volume_config.construct(*global_env());

	return _this;
}


AudioBootStrap GENODEAUD_bootstrap = {
	GENODEAUD_DRIVER_NAME, "Genode audio driver",
	GENODEAUD_Available, GENODEAUD_CreateDevice
};


static void GENODEAUD_WaitAudio(_THIS)
{
	Audio_out::Connection &con = *_this->hidden->audio[0];
	Audio_out::Packet     *p   =  _this->hidden->packet[0];

	unsigned const packet_pos = con.stream()->packet_position(p);
	unsigned const play_pos   = con.stream()->pos();
	unsigned queued           = packet_pos < play_pos
	                            ? ((Audio_out::QUEUE_SIZE + packet_pos) - play_pos)
	                            : packet_pos - play_pos;

	/* wait until there is only one packet left to play */
	while (queued > 1) {
		con.wait_for_progress();
		queued--;
	}
}


static void GENODEAUD_PlayAudio(_THIS)
{
	Genode::Lock_guard<Genode::Lock> guard(event_lock);

	Audio_out::Connection *c[AUDIO_CHANNELS];
	Audio_out::Packet     *p[AUDIO_CHANNELS];
	for (int channel = 0; channel < AUDIO_CHANNELS; channel++) {
		c[channel] = &(*_this->hidden->audio[channel]);
		p[channel] = _this->hidden->packet[channel];
	}

	/* get the currently played packet initially */
	static bool init = false;
	if (!init) {
		p[0] = c[0]->stream()->next();
		init = true;
	}

	float const volume = _this->hidden->volume_config->volume;

	/*
	 * Get new packet for left channel and use it to synchronize
	 * the right channel
	 */
	p[0] = c[0]->stream()->next(p[0]);
	unsigned ppos = c[0]->stream()->packet_position(p[0]);
	p[1] = c[1]->stream()->get(ppos);

	for (int sample = 0; sample < Audio_out::PERIOD; sample++)
		for (int channel = 0; channel < AUDIO_CHANNELS; channel++)
			p[channel]->content()[sample] =
				volume * (float)(((int16_t*)_this->hidden->mixbuf)[sample * AUDIO_CHANNELS + channel]) / 32768;

	for (int channel = 0; channel < AUDIO_CHANNELS; channel++) {
		_this->hidden->audio[channel]->submit(p[channel]);
		/*
		 * Save current packet to query packet position next time and
		 * when in GENODEAUD_WaitAudio
		 */
		_this->hidden->packet[channel] = p[channel];
	}
}


static Uint8 *GENODEAUD_GetAudioBuf(_THIS)
{
	return(_this->hidden->mixbuf);
}


static void GENODEAUD_CloseAudio(_THIS)
{
	if ( _this->hidden->mixbuf != NULL ) {
		SDL_FreeAudioMem(_this->hidden->mixbuf);
		_this->hidden->mixbuf = NULL;
	}
}


static int GENODEAUD_OpenAudio(_THIS, SDL_AudioSpec *spec)
{
	log("requested freq=",    spec->freq);
	log("          format=",  Hex(spec->format));
	log("          samples=", spec->samples);
	log("          size=",    spec->size);

	spec->channels = AUDIO_CHANNELS;
	spec->format = AUDIO_S16LSB;
	spec->freq = Audio_out::SAMPLE_RATE;
	spec->samples = Audio_out::PERIOD;
	SDL_CalculateAudioSpec(spec);

	/* Allocate mixing buffer */
	_this->hidden->mixlen = spec->size;
	_this->hidden->mixbuf = (Uint8 *) SDL_AllocAudioMem(_this->hidden->mixlen);
	if ( _this->hidden->mixbuf == NULL ) {
		return(-1);
	}
	SDL_memset(_this->hidden->mixbuf, spec->silence, spec->size);

	/* We're ready to rock and roll. :-) */
	return(0);
}

}
