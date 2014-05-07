/*
 * \brief  Audio_out Mixer
 * \author Sebastian Sumpf
 * \date   2012-12-20
 *
 * The mixer impelements the audio session on the server side. For each channel
 * (currently 'left' and 'right' only) it supports multiple client sessions and
 * mixes its input into a single client audio session.
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/lock.h>
#include <base/env.h>
#include <base/sleep.h>

#include <audio_out_session/rpc_object.h>
#include <audio_out_session/connection.h>
#include <cap_session/connection.h>
#include <root/component.h>
#include <timer_session/connection.h>

using namespace Genode;


static bool verbose = false;

enum Channel_number { LEFT, RIGHT, MAX_CHANNELS };


namespace Audio_out
{
	class Session_elem;
	class Session_component;
	class Root;
	class Channel;
	class Mixer;
}


static Audio_out::Channel *_channels[MAX_CHANNELS];


static int channel_number_from_string(const char *name)
{
	static struct Names {
		const char    *name;
		Channel_number number;
	} names[] = {
		{ "left", LEFT }, { "front left", LEFT },
		{ "right", RIGHT }, { "front right", RIGHT },
		{ 0, MAX_CHANNELS }
	};

	for (Names *n = names; n->name; ++n)
		if (!strcmp(name, n->name))
			return n->number;

		return -1;
}


/**
 * Makes audio a list element
 */
struct Audio_out::Session_elem : Audio_out::Session_rpc_object,
                                 List<Audio_out::Session_elem>::Element
{
	Session_elem(Signal_context_capability data_cap)
	: Session_rpc_object(data_cap) { }
};


/**
 * One channel containing multiple sessions
 */
class Audio_out::Channel
{
	private:

		List<Session_elem> _list;

	public:

		void insert(Session_elem *session) {
			_list.insert(session); }

		Session_elem *first() {
			return _list.first();
		}
};


/**
 * The mixer
 */
class Audio_out::Mixer : public Thread<1024 * sizeof(addr_t)>
{
	private:

		Lock _sleep_lock;

		Connection  _left;   /* left output */
		Connection  _right;  /* right output */
		Connection *_out[MAX_CHANNELS];

		Signal_receiver *_data_recv; /* data availble signal receiver 
		                               (send from clients) */

		void _sleep() { _sleep_lock.lock(); }

		Mixer()
		:
			Thread("audio_out_mixer"),
			_sleep_lock(Lock::LOCKED), _left("left", false, true),
			_right("right", false, true)
		{
			_out[LEFT]  = &_left;
			_out[RIGHT] = &_right;
			start();
		}

		/* check for active sessions */
		bool _check_active()
		{
			bool active = false;
			for (int i = 0; i < MAX_CHANNELS; i++) {
				Session_elem *session = _channels[i]->first();
				while (session) {
					active |= session->active();
					session = session->next();
				}
			}
			return active;
		}

		void _advance_session(Session_elem *session, unsigned pos)
		{
			if (session->stopped())
				return;

			Stream *stream = session->stream();
			bool full = stream->full();

			/* mark packets as played and icrement position pointer */
			while (stream->pos() != pos) {
				stream->get(stream->pos())->mark_as_played();
				stream->increment_position();
			}

			/* send 'progress' sginal */
			session->progress_submit();

			/* send 'alloc' signal */
			if (full)
				session->alloc_submit();
		}

		/* advance 'positions' of all client sessions */
		void _advance_position()
		{
			for (int i = 0; i < MAX_CHANNELS; i++) {
				Session_elem *session = _channels[i]->first();
				unsigned pos = _out[i]->stream()->pos();
				while (session) {
					_advance_session(session, pos);
					session = session->next();
				}
			}
		}

		/* mix one packet */
		void _mix_packet(Packet *out, Packet *in, bool clear)
		{
			/* when clear is set, set input an zero out remainder */
			if (clear) {
				out->content(in->content(), PERIOD);
			} else {
				/* mix */
				for (int i = 0; i < PERIOD; i++) {
					out->content()[i] += in->content()[i];

					if (out->content()[i] > 1) out->content()[i] = 1;
					if (out->content()[i] < -1) out->content()[i] = -1;
				}
			}
			/* mark packet as processed */
			in->invalidate();
		}

		/* get packet at offset */
		Packet *session_packet(Session *session, unsigned offset)
		{
			Stream *s = session->stream();
			return s->get(s->pos() + offset);
		}

		/* mix all session of one channel */
		bool _mix_channel(Channel_number nr, unsigned out_pos, unsigned offset)
		{
			Stream *out_stream = _out[nr]->stream();
			Packet *out        = out_stream->get(out_pos + offset);

			bool clear = true;
			bool mix_all = false;
			bool out_valid = out->valid();

			for (Session_elem *session = _channels[nr]->first();
			     session;
			     session = session->next()) {
			
				if (session->stopped())
					continue;

				Packet *in = session_packet(session, offset);

				/*
				 * When there already is an out packet, start over and mix
				 * everything.
				 */
				if (in->valid() && out_valid && !mix_all) {
					clear   = true;
					mix_all = true;
					session = _channels[nr]->first();
					in = session_packet(session, offset);
				}

				/* skip if packet has been processed or was already played */
				if ((!in->valid() && !mix_all) || in->played())
					continue;

				_mix_packet(out, in, clear);

				if (verbose)
					PDBG("mix: ch %u in %u -> out %u all %d o: %u",
					     nr, session->stream()->packet_position(in),
					     out_stream->packet_position(out), mix_all, offset);

				clear = false;
			}

			return !clear;
		}

		void _mix()
		{
			unsigned pos[MAX_CHANNELS];
			pos[LEFT] = _out[LEFT]->stream()->pos();
			pos[RIGHT] = _out[RIGHT]->stream()->pos();

			/*
			 * Look for packets that are valid, mix channels in an alternating
			 * way.
			 */
			for (int i = 0; i < QUEUE_SIZE; i++) {
				bool mix_one = true;
				for (int j = LEFT; j < MAX_CHANNELS; j++)
					mix_one &= _mix_channel((Channel_number)j, pos[j], i);

				if (mix_one) {
					_out[LEFT]->submit(_out[LEFT]->stream()->get(pos[LEFT] + i));
					_out[RIGHT]->submit(_out[RIGHT]->stream()->get(pos[RIGHT] + i));
				}
			}
		}

		#define FOREACH_CHANNEL(func) ({ \
			for (int i = 0; i < MAX_CHANNELS; i++) \
			_out[i]->func(); \
		})
		void _wait_for_progress() { FOREACH_CHANNEL(wait_for_progress); }
		void _start()             { FOREACH_CHANNEL(start); }
		void _stop()              { FOREACH_CHANNEL(stop); }
		
	public:

		void entry()
		{
			_start();

			while (true) {

				if (!_check_active()) {
					_stop();
					_sleep();
					_start();
					continue;
				}

				_mix();

				/* advance position of clients */
				_advance_position();

				if (!_left.stream()->empty())
					_wait_for_progress();
				else
					_data_recv->wait_for_signal();
			}
		}

		void wakeup() { _sleep_lock.unlock(); }

		/* sync client position with output position */
		void sync_pos(Channel_number channel, Stream *stream) {
			stream->pos(_out[channel]->stream()->pos()); }

		void data_recv(Signal_receiver *recv) { _data_recv = recv; }

		static Mixer *m()
		{
			static Mixer _m;
			return &_m;
		}
};


class Audio_out::Session_component : public Audio_out::Session_elem
{
	private:

		Channel_number _channel;

	public:

		Session_component(Channel_number            channel,
		                  Signal_context_capability data_cap)
		: Session_elem(data_cap), _channel(channel) { }

		void start()
		{
			Session_rpc_object::start();
			/* sync audio position with mixer */
			Mixer::m()->sync_pos(_channel, stream());
			Mixer::m()->wakeup();
		}
};


namespace Audio_out {
	typedef Root_component<Session_component, Multiple_clients> Root_component;
}


class Audio_out::Root : public Audio_out::Root_component
{
	Signal_context_capability _data_cap;

	Session_component *_create_session(const char *args)
	{
		char channel_name[16];
		Arg_string::find_arg(args, "channel").string(channel_name,
		                                             sizeof(channel_name),
		                                             "left");
		size_t ram_quota =
		Arg_string::find_arg(args, "ram_quota"  ).ulong_value(0);

		size_t session_size = align_addr(sizeof(Session_component), 12);

		if ((ram_quota < session_size) ||
		    (sizeof(Stream) > ram_quota - session_size)) {
				PERR("insufficient 'ram_quota', got %zd, need %zd",
				      ram_quota, sizeof(Stream) + session_size);
				throw Root::Quota_exceeded();
		}

		int ch = channel_number_from_string(channel_name);
		if (ch < 0)
			throw Root::Invalid_args();

		Session_component *session = new (md_alloc())
		                             Session_component((Channel_number)ch, _data_cap);

		PDBG("Added new \"%s\" nr: %u s: %p",channel_name, ch, session);
		_channels[ch]->insert(session);
		return session;
	}

	public:

		Root(Rpc_entrypoint           *session_ep,
		     Signal_context_capability data_cap,
		     Allocator                *md_alloc)
		: Root_component(session_ep, md_alloc), _data_cap(data_cap) { }
};


int main()
{
	static Cap_connection cap;
	enum { STACK_SIZE = 4*1024*sizeof(addr_t) }; 

	for (int i = 0; i < MAX_CHANNELS; i++)
		_channels[i] = new (env()->heap()) Audio_out::Channel();

	static Signal_receiver data_recv;
	static Signal_context  data_context;
	static Signal_context_capability data_cap(data_recv.manage(&data_context));

	/* start mixer thread */
	Audio_out::Mixer::m()->data_recv(&data_recv);

	static Rpc_entrypoint ep(&cap, STACK_SIZE, "audio_ep");
	static Audio_out::Root root(&ep, data_cap, env()->heap());
	env()->parent()->announce(ep.manage(&root));

	sleep_forever();
	return 0;
}

