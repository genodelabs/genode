/*
 * \brief  Audio_out-out driver for Linux
 * \author Christian Helmuth
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \date   2010-05-11
 *
 * FIXME session and driver shutdown not implemented (audio_drv_stop)
 */

/*
 * Copyright (C) 2010-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/env.h>
#include <base/sleep.h>
#include <root/component.h>
#include <cap_session/connection.h>
#include <audio_out_session/rpc_object.h>
#include <util/misc_math.h>
#include <os/config.h>
#include <os/server.h>
#include <timer_session/connection.h>

/* local includes */
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
class Audio_out::Out
{
	private:

		Server::Entrypoint                        &_ep;
		Genode::Signal_rpc_member<Audio_out::Out>  _data_avail_dispatcher;
		Genode::Signal_rpc_member<Audio_out::Out>  _timer_dispatcher;

		Timer::Connection _timer;

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

			/* convert float to S16LE */
			static short data[2 * PERIOD];

			if (p_left->valid() && p_right->valid()) {

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
			}

			_advance_position(p_left, p_right);

			return true;
		}

		void _handle_data_avail(unsigned num) { }

		void _handle_timer(unsigned)
		{
			if (_active()) _play_packet();
		}

	public:

		Out(Server::Entrypoint &ep)
		:
			_ep(ep),
			_data_avail_dispatcher(ep, *this, &Audio_out::Out::_handle_data_avail),
			_timer_dispatcher(ep, *this, &Audio_out::Out::_handle_timer)
		{
			_timer.sigh(_timer_dispatcher);

			unsigned const us = (Audio_out::PERIOD * 1000 / Audio_out::SAMPLE_RATE)*1000;
			_timer.trigger_periodic(us);
		}

		Signal_context_capability data_avail_sigh() { return _data_avail_dispatcher; }
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

		Root(Server::Entrypoint &ep, Allocator *md_alloc,
		     Signal_context_capability data_cap)
		: Root_component(&ep.rpc_ep(), md_alloc), _data_cap(data_cap)
		{ }
};


struct Main
{
	Server::Entrypoint &ep;

	Main(Server::Entrypoint &ep) : ep(ep)
	{
		char dev[32] = { 'h', 'w', 0 };
		try {
			Genode::Xml_node config = Genode::config()->xml_node();
			config.attribute("alsa_device").value(dev, sizeof(dev));
		} catch (...) { }

		/* init ALSA */
		int err = audio_drv_init(dev);
		if (err) {
			if (err == -1) PERR("Could not open ALSA device '%s'.", dev);
			else           PERR("Could not initialize driver, error: %d", err);

			throw -1;
		}
		audio_drv_start();

		static Audio_out::Out  out(ep);
		static Audio_out::Root root(ep, Genode::env()->heap(),
		                            out.data_avail_sigh());
		env()->parent()->announce(ep.manage(root));
		PINF("--- start Audio_out ALSA driver ---");
	}
};


/************
 ** Server **
 ************/

namespace Server {
	char const *name()                    { return "audio_drv_ep";        }
	size_t      stack_size()              { return 2*1024*sizeof(addr_t); }
	void        construct(Entrypoint &ep) { static Main main(ep);         }
}
