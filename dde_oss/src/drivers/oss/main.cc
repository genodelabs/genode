/*
 * \brief  Audio-out session entry point
 * \author Sebastian Sumpf
 * \date   2012-11-20
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

extern "C" {
#include <oss_config.h>
}

#include <base/env.h>
#include <base/sleep.h>
#include <root/component.h>
#include <cap_session/connection.h>
#include <audio_out_session/rpc_object.h>
#include <util/misc_math.h>

#include <audio.h>
#include <signal.h>

using namespace Genode;


static const bool verbose = false;
static bool audio_out_active = false;

enum Channel_number { LEFT, RIGHT, MAX_CHANNELS, INVALID = MAX_CHANNELS };


namespace Audio_out {
	class  Session_component;
	class  Out;
	class  Root;
	struct Root_policy;
	static Session_component *channel_acquired[MAX_CHANNELS];
}


class Audio_out::Session_component : public Audio_out::Session_rpc_object
{
	private:

		Channel_number            _channel;
		Signal_context_capability _ctx_cap;
		Signal_transmitter        _signal;

	public:

		Session_component(Channel_number channel, Signal_context_capability ctx_cap)
		:  Session_rpc_object(ctx_cap), _channel(channel), _ctx_cap(ctx_cap)
		{
			Audio_out::channel_acquired[_channel] = this;
			_signal.context(ctx_cap);
		}

		~Session_component()
		{
			Audio_out::channel_acquired[_channel] = 0;
		}

		void start()
		{
			Session_rpc_object::start();
			/* this will trigger Audio_out::Out::handle */
			_signal.submit();
		}
};


class Audio_out::Out : public Driver_context
{
	private:

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
			right()->pos(right()->packet_position(r));

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
			Packet *p_right = right()->get(right()->pos());

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

			/* send to driver */
			int err;
			if (audio_out_active)
				if ((err = audio_play(data, 4 * PERIOD)))
					PWRN("Error %d during playback", err);

			p_left->mark_as_played();
			p_right->mark_as_played();

			_advance_position(p_left, p_right);

			return true;
		}


	public:

		void handle()
		{
			while (true) {
				if (!_active()) {
					//TODO: may stop device
					return;
				}

				/* play one packet */
				if (!_play_packet())
					return;

				/* give others a try */
				Service_handler::s()->check_signal(false);
			}
		}

		const char *debug() { return "Audio out"; }
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
		if (!Genode::strcmp(name, n->name)) {
			*out_number = n->number;
			return true;
		}

	return false;
}


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


/**
 * Root component, handling new session requests.
 */
class Audio_out::Root : public Audio_out::Root_component
{
	private:

		Signal_context_capability _ctx_cap;

	protected:

		Session_component *_create_session(const char *args)
		{
			if (!audio_out_active)
				throw Root::Unavailable();

			char channel_name[16];
			Channel_number channel_number = INVALID;
			Arg_string::find_arg(args, "channel").string(channel_name,
			                                             sizeof(channel_name),
			                                             "left");
			channel_number_from_string(channel_name, &channel_number);

			return new (md_alloc())
				Session_component(channel_number, _ctx_cap);
		}

	public:

		Root(Rpc_entrypoint *session_ep, Allocator *md_alloc, Signal_context_capability ctx_cap)
		: Root_component(session_ep, md_alloc), _ctx_cap(ctx_cap)
		{ }
};


int main()
{
	static Signal_receiver recv;
	enum { STACK_SIZE = 1024 * sizeof(addr_t) };
	static Cap_connection cap;
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "audio_ep");

	dde_kit_timer_init(0, 0);

	Irq::init(&recv);
	Service_handler::s()->receiver(&recv);

	/* probe drivers */
	probe_drivers();
	audio_out_active = audio_init() ? false : true;

	if (audio_out_active) {
		static Audio_out::Out out;
		static Audio_out::Root audio_root(&ep, env()->heap(), recv.manage(&out));
		env()->parent()->announce(ep.manage(&audio_root));
	}

	while (true)
		Service_handler::s()->check_signal(true);

	return 1;
}

