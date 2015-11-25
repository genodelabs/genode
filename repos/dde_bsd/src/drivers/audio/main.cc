/*
 * \brief  Startup audio driver library
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
#include <audio_in_session/rpc_object.h>
#include <audio_out_session/rpc_object.h>
#include <base/env.h>
#include <base/sleep.h>
#include <cap_session/connection.h>
#include <os/config.h>
#include <os/server.h>
#include <root/component.h>
#include <util/misc_math.h>
#include <trace/timestamp.h>

/* local includes */
#include <audio/audio.h>


using namespace Genode;

using namespace Audio;


/**************
 ** Playback **
 **************/

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

		Channel_number _channel;

	public:

		Session_component(Channel_number channel, Signal_context_capability cap)
		:
			Session_rpc_object(cap), _channel(channel)
		{
			Audio_out::channel_acquired[_channel] = this;
		}

		~Session_component()
		{
			Audio_out::channel_acquired[_channel] = 0;
		}
};


class Audio_out::Out
{
	private:

		Server::Entrypoint                        &_ep;
		Genode::Signal_rpc_member<Audio_out::Out>  _data_avail_dispatcher;
		Genode::Signal_rpc_member<Audio_out::Out>  _notify_dispatcher;

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
		}

		void _play_silence()
		{
			static short silence[Audio_out::PERIOD * Audio_out::MAX_CHANNELS] = { 0 };

			int err = Audio::play(silence, sizeof(silence));
			if (err && err != 35)
				PWRN("Error %d during silence playback", err);
		}

		void _play_packet()
		{
			unsigned lpos = left()->pos();
			unsigned rpos = right()->pos();

			Packet *p_left  = left()->get(lpos);
			Packet *p_right = right()->get(rpos);

			if (p_left->valid() && p_right->valid()) {
				/* convert float to S16LE */
				static short data[Audio_out::PERIOD * Audio_out::MAX_CHANNELS];

				for (int i = 0; i < Audio_out::PERIOD * Audio_out::MAX_CHANNELS; i += 2) {
					data[i] = p_left->content()[i / 2] * 32767;
					data[i + 1] = p_right->content()[i / 2] * 32767;
				}

				/* send to driver */
				if (int err = Audio::play(data, sizeof(data)))
					PWRN("Error %d during playback", err);

				p_left->invalidate();
				p_right->invalidate();

				p_left->mark_as_played();
				p_right->mark_as_played();

			} else {
				_play_silence();
			}

			_advance_position(p_left, p_right);

			/* always report when a period has passed */
			Session_component *channel_left  = channel_acquired[LEFT];
			Session_component *channel_right = channel_acquired[RIGHT];
			channel_left->progress_submit();
			channel_right->progress_submit();
		}

		/*
		 * Data available in session buffer
		 *
		 * We do not care about this signal because we already have
		 * started to play and we will keep doing it, even if it is
		 * silence.
		 */
		void _handle_data_avail(unsigned) { }

		/*
		 * DMA block played
		 */
		void _handle_notify(unsigned)
		{
			if (_active())
				_play_packet();
		}

	public:

		Out(Server::Entrypoint &ep)
		:
			_ep(ep),
			_data_avail_dispatcher(ep, *this, &Audio_out::Out::_handle_data_avail),
			_notify_dispatcher(ep, *this, &Audio_out::Out::_handle_notify)
		{
			/* play a silence packet to get the driver running */
			_play_silence();
		}

		Signal_context_capability data_avail() { return _data_avail_dispatcher; }

		Signal_context_capability sigh() { return _notify_dispatcher; }

		static bool channel_number(const char     *name,
		                           Channel_number *out_number)
		{
			static struct Names {
				const char     *name;
				Channel_number  number;
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
		if (!Out::channel_number(channel_name, &channel_number))
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

		Server::Entrypoint &_ep;

		Signal_context_capability _cap;

	protected:

		Session_component *_create_session(const char *args)
		{
			char channel_name[16];
			Channel_number channel_number = INVALID;
			Arg_string::find_arg(args, "channel").string(channel_name,
			                                             sizeof(channel_name),
			                                             "left");
			Out::channel_number(channel_name, &channel_number);

			return new (md_alloc())
				Session_component(channel_number, _cap);
		}

	public:

		Root(Server::Entrypoint &ep, Allocator &md_alloc,
		     Signal_context_capability cap)
		:
			Root_component(&ep.rpc_ep(), &md_alloc),
			_ep(ep), _cap(cap)
		{ }
};


/***************
 ** Recording **
 ***************/

namespace Audio_in {

	class Session_component;
	class In;
	class Root;
	struct Root_policy;
	static Session_component *channel_acquired;

}


class Audio_in::Session_component : public Audio_in::Session_rpc_object
{
	private:

		Channel_number _channel;

	public:

		Session_component(Channel_number channel,
		                  Genode::Signal_context_capability cap)
		: Session_rpc_object(cap), _channel(channel) {
			channel_acquired = this; }

		~Session_component() { channel_acquired = nullptr; }
};


class Audio_in::In
{
	private:

		Server::Entrypoint                      &_ep;
		Genode::Signal_rpc_member<Audio_in::In>  _notify_dispatcher;

		bool _active() { return channel_acquired && channel_acquired->active(); }

		Stream *stream() { return channel_acquired->stream(); }

		void _record_packet()
		{
			static short data[2 * Audio_in::PERIOD];
			if (int err = Audio::record(data, sizeof(data))) {
					if (err && err != 35)
						PWRN("Error %d during recording", err);
					return;
			}

			/*
			 * Check for an overrun first and notify the client later.
			 */
			bool overrun = stream()->overrun();

			Packet *p = stream()->alloc();

			float const scale = 32768.0f * 2;

			float * const content = p->content();
			for (int i = 0; i < 2*Audio_in::PERIOD; i += 2) {
				float sample = data[i] + data[i+1];
				content[i/2] = sample / scale;
			}

			stream()->submit(p);

			channel_acquired->progress_submit();

			if (overrun) channel_acquired->overrun_submit();
		}

		void _handle_notify(unsigned)
		{
			if (_active())
				_record_packet();
		}

	public:

		In(Server::Entrypoint &ep)
		:
			_ep(ep),
			_notify_dispatcher(ep, *this, &Audio_in::In::_handle_notify)
		{ _record_packet(); }

		Signal_context_capability sigh() { return _notify_dispatcher; }

		static bool channel_number(const char     *name,
		                           Channel_number *out_number)
		{
			static struct Names {
				const char     *name;
				Channel_number  number;
			} names[] = {
				{ "left", LEFT },
				{ 0, INVALID }
			};

			for (Names *n = names; n->name; ++n)
				if (!Genode::strcmp(name, n->name)) {
					*out_number = n->number;
					return true;
				}

			return false;
		}
};


struct Audio_in::Root_policy
{
	void aquire(char const *args)
	{
		size_t ram_quota = Arg_string::find_arg(args, "ram_quota").ulong_value(0);
		size_t session_size = align_addr(sizeof(Audio_in::Session_component), 12);

		if ((ram_quota < session_size) ||
		    (sizeof(Stream) > (ram_quota - session_size))) {
			PERR("insufficient 'ram_quota', got %zu, need %zu",
			     ram_quota, sizeof(Stream) + session_size);
			throw Genode::Root::Quota_exceeded();
		}

		char channel_name[16];
		Channel_number channel_number;
		Arg_string::find_arg(args, "channel").string(channel_name,
		                                             sizeof(channel_name),
		                                             "left");
		if (!In::channel_number(channel_name, &channel_number))
			throw ::Root::Invalid_args();
		if (Audio_in::channel_acquired)
			throw Genode::Root::Unavailable();
	}

	void release() { }
};


namespace Audio_in {
	typedef Root_component<Session_component, Root_policy> Root_component;
}


/**
 * Root component, handling new session requests.
 */
class Audio_in::Root : public Audio_in::Root_component
{
	private:

		Server::Entrypoint        &_ep;
		Signal_context_capability  _cap;

	protected:

		Session_component *_create_session(char const *args)
		{
			char channel_name[16];
			Channel_number channel_number = INVALID;
			Arg_string::find_arg(args, "channel").string(channel_name,
			                                             sizeof(channel_name),
			                                             "left");
			In::channel_number(channel_name, &channel_number);
			return new (md_alloc()) Session_component(channel_number, _cap);
		}

	public:

		Root(Server::Entrypoint &ep, Allocator &md_alloc,
		     Signal_context_capability cap)
		: Root_component(&ep.rpc_ep(), &md_alloc), _ep(ep), _cap(cap) { }
};


/**********
 ** Main **
 **********/

static bool disable_playback()
{
	using namespace Genode;
	try {
		return config()->xml_node().attribute("playback").has_value("no");
	} catch (...) { }

	return false;
}


static bool enable_recording()
{
	using namespace Genode;
	try {
		return config()->xml_node().attribute("recording").has_value("yes");
	} catch (...) { }

	return false;
}


struct Main
{
	Server::Entrypoint &ep;

	Main(Server::Entrypoint &ep) : ep(ep)
	{
		Audio::init_driver(ep);

		if (Audio::driver_active()) {

			/* playback */
			if (!disable_playback()) {
				static Audio_out::Out out(ep);
				Audio::play_sigh(out.sigh());
				static Audio_out::Root out_root(ep, *env()->heap(), out.data_avail());
				env()->parent()->announce(ep.manage(out_root));
				PINF("--- BSD Audio driver enable playback ---");
			}

			/* recording */
			if (enable_recording()) {
				static Audio_in::In in(ep);
				Audio::record_sigh(in.sigh());
				static Audio_in::Root in_root(ep, *env()->heap(),
				                              Genode::Signal_context_capability());
				env()->parent()->announce(ep.manage(in_root));
				PINF("--- BSD Audio driver enable recording ---");
			}
		}
	}
};


/************
 ** Server **
 ************/

namespace Server {
	char const *name()             { return "audio_drv_ep";      }
	size_t      stack_size()       { return 8*1024*sizeof(long); }
	void construct(Entrypoint &ep) { static Main server(ep);     }
}
