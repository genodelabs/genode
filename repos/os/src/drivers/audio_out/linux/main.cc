/*
 * \brief  Audio_out-out driver for Linux
 * \author Christian Helmuth
 * \author Sebastian Sumpf
 * \date   2010-05-11
 *
 * FIXME session and driver shutdown not implemented (audio_drv_stop)
 *
 * 2013-01-09: Adapted to news audio interface
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
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

enum Channel_number { LEFT, RIGHT, MAX_CHANNELS, INVALID = MAX_CHANNELS };


namespace Audio_out
{
	class  Session_component;
	class  Out;
	class  Root;
	struct Root_policy;
	static Session_component *channel_acquired[MAX_CHANNELS];
};


class Audio_out::Session_component : public Audio_out::Session_rpc_object
{
	private:

		Channel_number           _channel;

	public:

		Session_component(Channel_number channel, Signal_context_capability data_cap)
		:
			Session_rpc_object(data_cap),
			_channel(channel)
		{
			Audio_out::channel_acquired[_channel] = this;
		}

		~Session_component()
		{
			Audio_out::channel_acquired[_channel] = 0;
		}
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



/*
 * Root component, handling new session requests.
 */
class Audio_out::Out : public Thread<1024 * sizeof(addr_t)>
{
	private:

		Signal_receiver *_data_recv;     /* data is available */
		
		bool _active() {
			return  channel_acquired[LEFT] && channel_acquired[RIGHT] &&
			        channel_acquired[LEFT]->active() && channel_acquired[RIGHT]->active();
		}

		Stream *left()  { return channel_acquired[LEFT]->stream(); }
		Stream *right() { return channel_acquired[RIGHT]->stream(); }

		void _advance_position(Packet *l, Packet *r)
		{
			bool full_left = left()->full();
			bool full_right = right()->full();

			left()->pos(left()->packet_position(l));
			right()->pos(right()->packet_position(l));

			left()->increment_position();
			right()->increment_position();

			Session_component *channel_left  = channel_acquired[LEFT];
			Session_component *channel_right = channel_acquired[RIGHT];

			if (full_left)
				channel_left->alloc_submit();

			if (full_right)
				channel_right->alloc_submit();

			channel_left->progress_submit();
			channel_right->progress_submit();
		}

		bool _play_packet()
		{
			Packet *p_left  = left()->get(left()->pos());
			Packet *p_right = right()->get(left()->pos());

			bool found = false;
			for (int i = 0; i < QUEUE_SIZE; i++) {
				if (p_left->valid() && p_right->valid()) {
					found = true;
					break;
				}

				p_left  = left()->next(p_left);
				p_right = right()->next(p_right);
			}

			if (!found)
				return false;

			/* convert float to S16LE */
			static short data[2 * PERIOD];

			for (int i = 0; i < 2 * PERIOD; i += 2) {
				data[i] = p_left->content()[i / 2] * 32767;
				data[i + 1] = p_right->content()[i / 2] * 32767;
			}

			p_left->invalidate();
			p_right->invalidate();

			if (verbose)
				PDBG("play packet");

			/* blocking-write packet to ALSA */
			while (int err = audio_drv_play(data, PERIOD)) {
				if (verbose) PERR("Error %d during playback", err);
					audio_drv_stop();
					audio_drv_start();
				}

			p_left->mark_as_played();
			p_right->mark_as_played();

			_advance_position(p_left, p_right);

			return true;
		}

	public:

		Out(Signal_receiver *data_recv)
		: Thread("audio_out"), _data_recv(data_recv) { }

		void entry()
		{
			audio_drv_adopt_myself();

			/* handle audio out */
			while (true)
				if (!_active() || !_play_packet())
					_data_recv->wait_for_signal();
		}
};


/**
 * Session creation policy for our service
 */
struct Audio_out::Root_policy
{
	void aquire(const char *args)
	{
		size_t ram_quota =
			Arg_string::find_arg(args, "ram_quota"  ).ulong_value(0);
		size_t session_size =
			align_addr(sizeof(Audio_out::Session_component), 12);

		if ((ram_quota < session_size) ||
		    (sizeof(Stream) > ram_quota - session_size)) {
			PERR("insufficient 'ram_quota', got %zd, need %zd",
			     ram_quota, sizeof(Stream) + session_size);
			throw ::Root::Quota_exceeded();
		}

		char channel_name[16];
		Channel_number channel_number;
		Arg_string::find_arg(args, "channel").string(channel_name,
		                                             sizeof(channel_name),
		                                             "left");
		if (!channel_number_from_string(channel_name, &channel_number))
			throw ::Root::Invalid_args();
		if (Audio_out::channel_acquired[channel_number])
			throw ::Root::Unavailable();
	}

	void release() { }
};


namespace Audio_out {
	typedef Root_component<Session_component, Root_policy> Root_component;
}


class Audio_out::Root : public Audio_out::Root_component
{
	private:

		Signal_context_capability _data_cap;

	protected:

		Session_component *_create_session(const char *args)
		{
			char channel_name[16];
			Channel_number channel_number = INVALID;
			Arg_string::find_arg(args, "channel").string(channel_name,
			                                             sizeof(channel_name),
			                                             "left");
			channel_number_from_string(channel_name, &channel_number);

			return new (md_alloc())
				Session_component(channel_number, _data_cap);
		}

	public:

		Root(Rpc_entrypoint *session_ep, Allocator *md_alloc, Signal_context_capability data_cap)
		: Root_component(session_ep, md_alloc), _data_cap(data_cap)
		{ }
};


/*
 * Manually initialize the 'lx_environ' pointer, needed because 'nic_drv' is
 * not using the normal Genode startup code.
 */
extern char **environ;
char **lx_environ = environ;


int main(int argc, char **argv)
{
	/* setup data available signal */
	static Signal_receiver data_recv;
	static Signal_context  data_context;
	static Signal_context_capability data_cap(data_recv.manage(&data_context));

	/* init ALSA */
	int err = audio_drv_init();
	if (err) {
		PERR("audio driver init returned %d", err);
		return 0;
	}
	audio_drv_start();

	/* start output thread */
	Audio_out::Out *out = new(env()->heap()) Audio_out::Out(&data_recv);
	out->start();

	enum { STACK_SIZE = 1024 * sizeof(addr_t) };
	static Cap_connection cap;
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "audio_ep");

	/* setup service */
	static Audio_out::Root audio_root(&ep, env()->heap(), data_cap);
	env()->parent()->announce(ep.manage(&audio_root));

	sleep_forever();
	return 0;
}

