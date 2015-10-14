/*
 * \brief  Audio_out Mixer
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \date   2012-12-20
 *
 * The mixer impelements the audio session on the server side. For each channel
 * (currently 'left' and 'right' only) it supports multiple client sessions and
 * mixes its input into a single client audio session.
 */

/*
 * Copyright (C) 2009-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <os/server.h>
#include <root/component.h>
#include <util/retry.h>
#include <util/string.h>
#include <audio_out_session/connection.h>
#include <audio_out_session/rpc_object.h>
#include <cap_session/connection.h>
#include <timer_session/connection.h>


static bool const verbose = false;

enum Channel_number { INVALID = -1, LEFT, RIGHT, MAX_CHANNELS };

static struct Names {
	 char const    *name;
	Channel_number  number;
} names[] = {
	{ "left", LEFT }, { "front left", LEFT },
	{ "right", RIGHT }, { "front right", RIGHT },
	{ nullptr, MAX_CHANNELS }
};


static Channel_number channel_number_from_string(char const *name)
{
	for (Names *n = names; n->name; ++n)
		if (!Genode::strcmp(name, n->name))
			return n->number;

	return INVALID;
}


static char const *channel_string_from_number(Channel_number ch)
{
	for (Names *n = names; n->name; ++n)
		if (ch == n->number)
			return n->name;

	return nullptr;
}

/**
 * Helper method for walking over arrays
 */
template <typename FUNC>
static void for_each_index(int max_index, FUNC const &func) {
	for (int i = 0; i < max_index; i++) func(i); }



namespace Audio_out
{
	class Session_elem;
	class Session_component;
	class Root;
	class Mixer;
}


enum { MAX_CHANNEL_NAME_LEN = 16, MAX_LABEL_LEN = 128 };


/**
 * Each session component is part of a list
 */
struct Audio_out::Session_elem : Audio_out::Session_rpc_object,
                                 Genode::List<Audio_out::Session_elem>::Element
{
	typedef Genode::String<MAX_LABEL_LEN> Label;

	Label label;

	Session_elem(char const *label, Genode::Signal_context_capability data_cap)
	: Session_rpc_object(data_cap), label(label) { }

	Packet *get_packet(unsigned offset) {
		return stream()->get(stream()->pos() + offset); }
};


/**
 * The mixer
 */
class Audio_out::Mixer
{
	private:

		Genode::Signal_rpc_member<Audio_out::Mixer> _dispatcher;

		Connection  _left;   /* left output */
		Connection  _right;  /* right output */
		Connection *_out[MAX_CHANNELS];

		/**
		 * Remix all exception
		 */
		struct Remix_all { };

		/**
		 * A channel contains multiple session components
		 */
		struct Channel : public Genode::List<Session_elem>
		{
			void insert(Session_elem *session) { List<Session_elem>::insert(session); }
			void remove(Session_elem *session) { List<Session_elem>::remove(session); }

			template <typename FUNC> void for_each_session(FUNC const &func)
			{
				for (Session_elem *elem = List<Session_elem>::first(); elem;
				     elem = elem->next()) func(*elem);
			}
		} _channels[MAX_CHANNELS];

		/**
		 * Helper method for walking over session in a channel
		 */
		template <typename FUNC> void _for_each_channel(FUNC const &func) {
			for (int i = LEFT; i < MAX_CHANNELS; i++) func(&_channels[i]); }

		bool _check_active()
		{
			bool active = false;
			_for_each_channel([&] (Channel *channel) {
				channel->for_each_session([&] (Session_elem const &session) {
					active |= session.active();
				});
			});
			return active;
		}

		void _advance_session(Session_elem *session, unsigned pos)
		{
			if (session->stopped()) return;

			Stream *stream  = session->stream();
			bool const full = stream->full();

			/* mark packets as played and icrement position pointer */
			while (stream->pos() != pos) {
				stream->get(stream->pos())->mark_as_played();
				stream->increment_position();
			}

			session->progress_submit();

			if (full) session->alloc_submit();
		}

		void _advance_position()
		{
			for_each_index(MAX_CHANNELS, [&] (int i) {
				unsigned const pos = _out[i]->stream()->pos();
				_channels[i].for_each_session([&] (Session_elem &session) {
					_advance_session(&session, pos);
				});
			});
		}

		void _mix_packet(Packet *out, Packet *in, bool clear)
		{
			if (clear) {
				out->content(in->content(), Audio_out::PERIOD);
			} else {
				for_each_index(Audio_out::PERIOD, [&] (int const i) {
					out->content()[i] += in->content()[i];

					if (out->content()[i] > 1) out->content()[i] = 1;
					if (out->content()[i] < -1) out->content()[i] = -1;
				});
			}

			/*
			 * Mark the packet as processed by invalidating it
			 */
			in->invalidate();
		}

		bool _mix_channel(Channel_number nr, unsigned out_pos, unsigned offset)
		{
			Stream  * const stream  = _out[nr]->stream();
			Packet  * const out     = stream->get(out_pos + offset);
			Channel * const channel = &_channels[nr];

			bool       clear     = true;
			bool       mix_all   = false;
			bool const out_valid = out->valid();

			Genode::retry<Remix_all>(
				[&] () {
					channel->for_each_session([&] (Session_elem &session) {
						if (session.stopped()) return;

						Packet *in = session.get_packet(offset);

						/*
						 * When there already is an out packet, start over and mix
						 * everything.
						 */
						if (in->valid() && out_valid && !mix_all) throw Remix_all();

						/* skip if packet has been processed or was already played */
						if ((!in->valid() && !mix_all) || in->played()) return;

						_mix_packet(out, in, clear);

						if (verbose)
							PLOG("mix: ch %u in %u -> out %u all %d o: %u",
							     nr, session.stream()->packet_position(in),
							     stream->packet_position(out), mix_all, offset);

						clear = false;
					});
				},
				[&] () {
					clear   = true;
					mix_all = true;
				});

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
			for_each_index(Audio_out::QUEUE_SIZE, [&] (int const i) {
				bool mix_one = true;
				for_each_index(MAX_CHANNELS, [&] (int const j) {
					mix_one = _mix_channel((Channel_number)j, pos[j], i);
				});

				if (mix_one) {
					_out[LEFT]->submit(_out[LEFT]->stream()->get(pos[LEFT] + i));
					_out[RIGHT]->submit(_out[RIGHT]->stream()->get(pos[RIGHT] + i));
				}
			});
		}

		/**
		 * Handle progress signals from Audio_out session and data available signals
		 * from each mixer client
		 */
		void _handle(unsigned)
		{
			_advance_position();
			_mix();
		}

	public:

		Mixer(Server::Entrypoint &ep)
		:
			_dispatcher(ep, *this, &Audio_out::Mixer::_handle),
			_left("left", false, true),
			_right("right", false, true)
		{
			_out[LEFT]  = &_left;
			_out[RIGHT] = &_right;
		}

		void start()
		{
			_out[LEFT]->progress_sigh(_dispatcher);
			for_each_index(MAX_CHANNELS, [&] (int const i) { _out[i]->start(); });
		}

		void stop()
		{
			for_each_index(MAX_CHANNELS, [&] (int const i) { _out[i]->stop(); });
			_out[LEFT]->progress_sigh(Genode::Signal_context_capability());
		}

		unsigned pos(Channel_number channel) const { return _out[channel]->stream()->pos(); }

		void add_session(Channel_number ch, Session_elem &session)
		{
			PLOG("add label: \"%s\" channel: \"%s\" nr: %u",
			     session.label.string(), channel_string_from_number(ch), ch);
			_channels[ch].insert(&session);
		}

		void remove_session(Channel_number ch, Session_elem &session)
		{
			PLOG("remove label: \"%s\" channel: \"%s\" nr: %u",
			     session.label.string(), channel_string_from_number(ch), ch);
			_channels[ch].remove(&session);
		}

		Genode::Signal_context_capability sig_cap() { return _dispatcher; }
};


class Audio_out::Session_component : public Audio_out::Session_elem
{
	private:

		Channel_number  _channel;
		Mixer          &_mixer;

	public:

		Session_component(char const     *label,
		                  Channel_number  channel,
		                  Mixer          &mixer)
		: Session_elem(label, mixer.sig_cap()), _channel(channel), _mixer(mixer)
		{
			_mixer.add_session(_channel, *this);
		}

		~Session_component()
		{
			if (Session_rpc_object::active()) stop();
			_mixer.remove_session(_channel, *this);
		}

		void start()
		{
			Session_rpc_object::start();
			/* sync audio position with mixer */
			stream()->pos(_mixer.pos(_channel));
		}

		void stop() { Session_rpc_object::stop(); }
};


namespace Audio_out {
	typedef Genode::Root_component<Session_component, Genode::Multiple_clients> Root_component;
}


class Audio_out::Root : public Audio_out::Root_component
{
	private:

		Mixer &_mixer;
		int    _sessions = { 0 };

		Session_component *_create_session(const char *args)
		{
			using namespace Genode;

			char label[MAX_LABEL_LEN];
			Arg_string::find_arg(args, "label").string(label,
			                                           sizeof(label),
			                                           "<noname>");

			char channel_name[MAX_CHANNEL_NAME_LEN];
			Arg_string::find_arg(args, "channel").string(channel_name,
			                                             sizeof(channel_name),
			                                             "left");

			size_t ram_quota =
				Arg_string::find_arg(args, "ram_quota").ulong_value(0);

			size_t session_size = align_addr(sizeof(Session_component), 12);

			if ((ram_quota < session_size) ||
			    (sizeof(Stream) > ram_quota - session_size)) {
				PERR("insufficient 'ram_quota', got %zu, need %zu",
				     ram_quota, sizeof(Stream) + session_size);
				throw Root::Quota_exceeded();
			}

			Channel_number ch = channel_number_from_string(channel_name);
			if (ch == INVALID)
				throw Root::Invalid_args();

			Session_component *session = new (md_alloc())
				Session_component(label, (Channel_number)ch, _mixer);

			if (++_sessions == 1) _mixer.start();
			return session;

		}

		void _destroy_session(Session_component *session)
		{
			if (--_sessions == 0) _mixer.stop();
			destroy(md_alloc(), session);
		}

	public:

		Root(Server::Entrypoint &ep,
		     Mixer              &mixer,
		     Genode::Allocator  &md_alloc)
		: Root_component(&ep.rpc_ep(), &md_alloc), _mixer(mixer) { }
};


/************
 ** Server **
 ************/

namespace Server { struct Main; }


struct Server::Main
{
	Server::Entrypoint &ep;

	Audio_out::Mixer mixer = { ep };
	Audio_out::Root  root  = { ep, mixer, *Genode::env()->heap() };

	Main(Server::Entrypoint &ep) : ep(ep)
	{
		Genode::env()->parent()->announce(ep.manage(root));
		PINF("--- Mixer started ---");
	}
};


namespace Server {
	char const *name()             { return "mixer_ep";            }
	size_t      stack_size()       { return 4*1024*sizeof(addr_t); }
	void construct(Entrypoint &ep) { static Main server(ep);       }
}
