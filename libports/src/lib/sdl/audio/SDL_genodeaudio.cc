/*
 * \brief  Genode-specific audio backend
 * \author Christian Prochaska
 * \date   2012-03-13
 *
 * based on the dummy SDL audio driver
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/allocator_avl.h>
#include <base/printf.h>
#include <base/thread.h>
#include <audio_out_session/connection.h>
#include <os/config.h>

enum {
	AUDIO_OUT_SAMPLE_SIZE = sizeof(float),
	AUDIO_OUT_CHANNELS = 2,
	AUDIO_OUT_FREQ = 44100,
	AUDIO_OUT_SAMPLES = 1024,
};

using Genode::env;
using Genode::Allocator_avl;
using Genode::Signal_context;
using Genode::Signal_receiver;

static const char *channel_names[] = { "front left", "front right" };
static float volume = 1.0;
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

typedef Audio_out::Session::Channel::Source Stream;


struct SDL_PrivateAudioData {
	Uint8 *mixbuf;
	Uint32 mixlen;
	Genode::Allocator_avl *block_alloc[AUDIO_OUT_CHANNELS];
	Audio_out::Connection *audio_out[AUDIO_OUT_CHANNELS];
	Stream *stream[AUDIO_OUT_CHANNELS];
};


/*
 * The first 'Signal_receiver' object in a process creates a signal receiver
 * thread. Currently this must not happen before the main program has started
 * or else the thread's context area would get overmapped on Genode/Linux when
 * the main program calls 'main_thread_bootstrap()' from '_main()'.
 */
static Signal_receiver *signal_receiver()
{
	static Signal_receiver _signal_receiver;
	return &_signal_receiver;
}


static void read_config()
{
	/* read volume from config file */
	try {
		unsigned int config_volume;
		Genode::config()->xml_node().sub_node("sdl_audio_volume").attribute("value").value(&config_volume);
		volume = (float)config_volume / 100;
	}
	catch (Genode::Config::Invalid) { }
	catch (Genode::Xml_node::Nonexistent_sub_node) { }
	catch (Genode::Xml_node::Nonexistent_attribute) { }
}


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
	for (int channel = 0; channel < AUDIO_OUT_CHANNELS; channel++) {
		destroy(env()->heap(), device->hidden->audio_out[channel]);
		destroy(env()->heap(), device->hidden->block_alloc[channel]);
	}

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
	for (int channel = 0; channel < AUDIO_OUT_CHANNELS; channel++) {
		_this->hidden->block_alloc[channel] = new (env()->heap()) Allocator_avl(env()->heap());
		try {
			_this->hidden->audio_out[channel] = new (env()->heap())
				Audio_out::Connection(channel_names[channel], _this->hidden->block_alloc[channel]);
			_this->hidden->stream[channel] = _this->hidden->audio_out[channel]->stream();
		} catch(Genode::Parent::Service_denied) {
			PERR("Could not connect to 'Audio_out' service.");
			destroy(env()->heap(), _this->hidden->block_alloc[channel]);
			while(--channel > 0) {
				destroy(env()->heap(), _this->hidden->audio_out[channel]);
				destroy(env()->heap(), _this->hidden->block_alloc[channel]);
			}
			return NULL;
		}
		if (channel > 0)
			_this->hidden->audio_out[channel]->sync_session(_this->hidden->audio_out[channel-1]->session_capability());
	}

	Genode::config()->sigh(signal_receiver()->manage(&config_signal_context));
	read_config();

	return _this;
}


AudioBootStrap GENODEAUD_bootstrap = {
	GENODEAUD_DRIVER_NAME, "Genode audio driver",
	GENODEAUD_Available, GENODEAUD_CreateDevice
};


/* This function waits until it is possible to write a full sound buffer */
static void GENODEAUD_WaitAudio(_THIS)
{
	for (int channel = 0; channel < AUDIO_OUT_CHANNELS; channel++)
		while (_this->hidden->stream[channel]->ack_avail() ||
		       !_this->hidden->stream[channel]->ready_to_submit()) {
			_this->hidden->stream[channel]->release_packet(_this->hidden->stream[channel]->get_acked_packet());
		}
}


static void GENODEAUD_PlayAudio(_THIS)
{
	Packet_descriptor p[AUDIO_OUT_CHANNELS];

	for (int channel = 0; channel < AUDIO_OUT_CHANNELS; channel++)
		while (1)
			try {
				p[channel] = _this->hidden->stream[channel]->alloc_packet(AUDIO_OUT_SAMPLE_SIZE * AUDIO_OUT_SAMPLES);
				break;
			} catch (Stream::Packet_alloc_failed) {
				/* wait for next finished packet */
				_this->hidden->stream[channel]->release_packet(_this->hidden->stream[channel]->get_acked_packet());
			}

	if (signal_receiver()->pending()) {
		signal_receiver()->wait_for_signal();
		Genode::config()->reload();
		read_config();
	}

	for (int sample = 0; sample < AUDIO_OUT_SAMPLES; sample++)
		for (int channel = 0; channel < AUDIO_OUT_CHANNELS; channel++)
			_this->hidden->stream[channel]->packet_content(p[channel])[sample] =
				volume * (float)(((int16_t*)_this->hidden->mixbuf)[sample * AUDIO_OUT_CHANNELS + channel]) / 32768;

	for (int channel = 0; channel < AUDIO_OUT_CHANNELS; channel++)
		_this->hidden->stream[channel]->submit_packet(p[channel]);
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
	PDBG("requested freq = %d", spec->freq);
	PDBG("requested format = 0x%x", spec->format);
	PDBG("requested samples = %u", spec->samples);
	PDBG("requested size = %u", spec->size);

	spec->channels = AUDIO_OUT_CHANNELS;
	spec->format = AUDIO_S16LSB;
	spec->freq = AUDIO_OUT_FREQ;
	spec->samples = AUDIO_OUT_SAMPLES;
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
