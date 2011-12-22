/*
 * \brief  Audio-out driver for Linux
 * \author Christian Helmuth
 * \date   2010-05-11
 *
 * FIXME session and driver shutdown not implemented (audio_drv_stop)
 */

/*
 * Copyright (C) 2010-2011 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/env.h>
#include <base/sleep.h>
#include <root/component.h>
#include <cap_session/connection.h>
#include <audio_out_session/rpc_object.h>
#include <util/misc_math.h>

#include "alsa.h"

using namespace Genode;

static const bool verbose = false;

namespace Audio_out {

	class Session_component;

	enum Channel_number { LEFT, RIGHT, MAX_CHANNELS, INVALID = MAX_CHANNELS };

	static Session_component *channel_acquired[MAX_CHANNELS];
	static Semaphore          channel_sema;


	class Session_component : public Session_rpc_object
	{
		private:

			Ram_dataspace_capability _ds;
			Channel_number           _channel;

			Ram_dataspace_capability _alloc_dataspace(size_t size)
			{
				_ds = env()->ram_session()->alloc(size);
				return _ds;
			}

		public:

			Session_component(Channel_number channel, size_t buffer_size,
			                  Rpc_entrypoint &ep)
			:
				Session_rpc_object(_alloc_dataspace(buffer_size), ep),
				_channel(channel)
			{
				Audio_out::channel_acquired[_channel] = this;
				channel_sema.up();
			}

			~Session_component()
			{
				channel_sema.down();
				Audio_out::channel_acquired[_channel] = 0;

				env()->ram_session()->free(_ds);
			}


			/*********************************
			 ** Audio-out session interface **
			 *********************************/

			void flush()
			{
				while (channel()->packet_avail())
					channel()->acknowledge_packet(channel()->get_packet());
			}

			void sync_session(Session_capability audio_out_session) { }
		};

	static bool channel_number_from_string(const char     *name,
	                                       Channel_number *out_number)
	{
		static struct Names {
			const char    *name;
			Channel_number number;
		} names[] = {
			{ "left", LEFT }, { "front left", LEFT },
			{ "right", RIGHT }, { "front right", RIGHT },
			{ 0, INVALID }
		};

		for (Names *n = names; n->name; ++n)
			if (!strcmp(name, n->name)) {
				*out_number = n->number;
				return true;
			}

		return false;
	}

	/**
	 * Session creation policy for our service
	 */
	struct Root_policy
	{
		void aquire(const char *args)
		{
			size_t ram_quota =
				Arg_string::find_arg(args, "ram_quota"  ).ulong_value(0);
			size_t buffer_size =
				Arg_string::find_arg(args, "buffer_size").ulong_value(0);
			size_t session_size =
				align_addr(sizeof(Session_component), 12);

			if ((ram_quota < session_size) ||
			    (buffer_size > ram_quota - session_size)) {
				PERR("insufficient 'ram_quota', got %zd, need %zd",
				     ram_quota, buffer_size + session_size);
				throw Root::Quota_exceeded();
			}

			char channel_name[16];
			Channel_number channel_number;
			Arg_string::find_arg(args, "channel").string(channel_name,
			                                             sizeof(channel_name),
			                                             "left");
			if (!channel_number_from_string(channel_name, &channel_number))
				throw Root::Invalid_args();
			if (channel_acquired[channel_number])
				throw Root::Unavailable();
		}

		void release() { }
	};

	typedef Root_component<Session_component, Root_policy> Root_component;

	/*
	 * Root component, handling new session requests.
	 */
	class Root : public Root_component,
	             private Thread<0x2000>
	{
		private:

			Semaphore       _startup_sema;  /* thread startup sync */
			Rpc_entrypoint &_channel_ep;

			bool _packets_available(Session_component *channel)
			{
				if (channel && channel->channel()->packet_avail())
					return true;
				else
					return false;
			}

			void entry()
			{
				audio_drv_adopt_myself();

				/* indicate thread-startup completion */
				_startup_sema.up();

				/* handle audio-out packets */
				while (true) {
					for (int i = 0; i < MAX_CHANNELS; ++i)
						channel_sema.down();

					Session_component *left  = channel_acquired[LEFT],
					                  *right = channel_acquired[RIGHT];

					/* get packets for channels */
					Packet_descriptor p[MAX_CHANNELS];
					static short data[2 * PERIOD];

					p[LEFT] = left->channel()->get_packet();
					for (int i = 0; i < 2 * PERIOD; i += 2)
						data[i] = left->channel()->packet_content(p[LEFT])[i / 2] * 32767;
					p[RIGHT] = right->channel()->get_packet();
					for (int i = 1; i < 2 * PERIOD; i += 2)
						data[i] = right->channel()->packet_content(p[RIGHT])[i / 2] * 32767;

					if (verbose)
						PDBG("play packet");

					/* blocking-write packet to ALSA */
					while (int err = audio_drv_play(data, PERIOD)) {
						if (verbose) PERR("Error %d during playback", err);
						audio_drv_stop();
						audio_drv_start();
					}

					/* acknowledge packet to the client */
					if (p[LEFT].valid())
						left->channel()->acknowledge_packet(p[LEFT]);
					if (p[RIGHT].valid())
						right->channel()->acknowledge_packet(p[RIGHT]);

					for (int i = 0; i < MAX_CHANNELS; ++i)
						channel_sema.up();
				}

			}

		protected:

			Session_component *_create_session(const char *args)
			{
				size_t buffer_size =
					Arg_string::find_arg(args, "buffer_size").ulong_value(0);

				char channel_name[16];
				Channel_number channel_number = INVALID;
				Arg_string::find_arg(args, "channel").string(channel_name,
				                                             sizeof(channel_name),
				                                             "left");
				channel_number_from_string(channel_name, &channel_number);

				return new (md_alloc())
					Session_component(channel_number, buffer_size, _channel_ep);
			}

		public:

			Root(Rpc_entrypoint *session_ep, Allocator *md_alloc)
			: Root_component(session_ep, md_alloc), _channel_ep(*session_ep)
			{
				/* synchronize with root thread startup */
				start();
				_startup_sema.down();
			}
	};
}


/*
 * Manually initialize the 'lx_environ' pointer, needed because 'nic_drv' is not
 * using the normal Genode startup code.
 */
extern char **environ;
char **lx_environ = environ;


int main(int argc, char **argv)
{
	enum { STACK_SIZE = 4096 };
	static Cap_connection cap;
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "audio_ep");

	/* init ALSA */
	int err = audio_drv_init();
	if (err) {
		PERR("audio driver init returned %d", err);
		return 0;
	}
	audio_drv_start();

	/* setup service */
	static Audio_out::Root audio_root(&ep, env()->heap());
	env()->parent()->announce(ep.manage(&audio_root));
	sleep_forever();
	return 0;
}

