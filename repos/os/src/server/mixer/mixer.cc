/*
 * \brief  Audio_out Mixer
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \date   2012-12-20
 *
 * The mixer implements the audio session on the server side. For each channel
 * (currently 'left' and 'right' only) it supports multiple client sessions and
 * mixes all input sessions into a single client audio output session.
 *
 *
 * There is a session list (Mixer::Session_channel) for each output channel that
 * contains multiple input sessions (Audio_out::Session_elem). For every packet
 * in the output queue the mixer sums the corresponding packets from all input
 * sessions up. The volume level of an input packet is applied in a linear way
 * (sample_value * volume_level) and the output packet is clipped at [1.0,-1.0].
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <mixer/channel.h>
#include <os/reporter.h>
#include <root/component.h>
#include <util/retry.h>
#include <util/string.h>
#include <util/xml_node.h>
#include <audio_out_session/connection.h>
#include <audio_out_session/rpc_object.h>
#include <timer_session/connection.h>
#include <base/attached_rom_dataspace.h>
#include <base/heap.h>
#include <base/component.h>
#include <base/log.h>


static bool verbose = false;

template <typename... ARGS>
static inline void logv(ARGS&&... args)
{
	if (verbose)
		Genode::log(args...);
}


typedef Mixer::Channel Channel;


enum {
	LEFT         = Channel::Number::LEFT,
	RIGHT        = Channel::Number::RIGHT,
	MAX_CHANNELS = Channel::Number::MAX_CHANNELS,
	MAX_VOLUME   = Channel::Volume_level::MAX,
};


static struct Names {
	 char const    *name;
	Channel::Number  number;
} names[] = {
	{ "left",        (Channel::Number)LEFT },
	{ "front left",  (Channel::Number)LEFT },
	{ "right",       (Channel::Number)RIGHT },
	{ "front right", (Channel::Number)RIGHT },
	{ nullptr,       (Channel::Number)MAX_CHANNELS }
};


static Channel::Number number_from_string(char const *name)
{
	for (Names *n = names; n->name; ++n)
		if (!Genode::strcmp(name, n->name))
			return n->number;
	return Channel::Number::INVALID;
}


static char const *string_from_number(Channel::Number ch)
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

	enum { MAX_CHANNEL_NAME_LEN = 16, MAX_LABEL_LEN = 128 };
	typedef Genode::String<MAX_LABEL_LEN> Label;
}


/**
 * The actual session element
 *
 * It is part of the Audio_out::Session_component implementation but since
 * it is also used by the mixer we defined it here.
 */
struct Audio_out::Session_elem : Audio_out::Session_rpc_object,
                                 Genode::List<Audio_out::Session_elem>::Element
{
	Label           label;
	Channel::Number number;
	float           volume { 0.f };
	bool            muted  { true };

	Session_elem(Genode::Env & env,
	             char const *label, Genode::Signal_context_capability data_cap)
	: Session_rpc_object(env, data_cap), label(label) { }

	Packet *get_packet(unsigned offset) {
		return stream()->get(stream()->pos() + offset); }
};


/**
 * The mixer
 */
class Audio_out::Mixer
{
	private:

		Genode::Env &env;

		Genode::Attached_rom_dataspace _config_rom { env, "config" };

		/*
		 * Mixer output Audio_out connection
		 */
		Connection  _left  { env, "left",  false, true };
		Connection  _right { env, "right", false, true };;
		Connection *_out[MAX_CHANNELS];
		float       _out_volume[MAX_CHANNELS];

		/*
		 * Default settings used as fallback for new sessions
		 */
		float _default_out_volume { 0.f };
		float _default_volume     { 0.f };
		bool  _default_muted      { true };

		/**
		 * Remix all exception
		 */
		struct Remix_all { };

		/**
		 * A channel contains multiple session components
		 */
		struct Session_channel : public Genode::List<Session_elem>
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
		template <typename FUNC> void _for_each_channel(FUNC const &func)
		{
			for (int i = LEFT; i < MAX_CHANNELS; i++)
				func((Channel::Number)i, &_channels[i]);
		}

		/*
		 * Channel reporter
		 *
		 * Each session in a channel is reported as an input node.
		 */
		Genode::Reporter reporter { env, "channel_list" };

		/**
		 * Report available channels
		 *
		 * This method is called if a new session is added or an old one
		 * removed as well as when the mixer configuration changes.
		 */
		void _report_channels()
		{
			reporter.enabled(true);

			try {
				Genode::Reporter::Xml_generator xml(reporter, [&] () {
					/* output channels */
					for_each_index(MAX_CHANNELS, [&] (int const i) {
						Channel::Number const num = (Channel::Number)i;
						char const * const   name = string_from_number(num);
						int const             vol = (int)(MAX_VOLUME * _out_volume[i]);

						xml.node("channel", [&] () {
							xml.attribute("type",  "output");
							xml.attribute("label", "master");
							xml.attribute("name",   name);
							xml.attribute("number", (int)num);
							xml.attribute("volume", (int)vol);
							xml.attribute("muted",  (long)0);
						});
					});

					/* input channels */
					_for_each_channel([&] (Channel::Number num, Session_channel *sc) {
						sc->for_each_session([&] (Session_elem const &session) {
							char const * const name = string_from_number(num);
							int const           vol = (int)(MAX_VOLUME * session.volume);

							xml.node("channel", [&] () {
								xml.attribute("type",   "input");
								xml.attribute("label",  session.label.string());
								xml.attribute("name",   name);
								xml.attribute("number", session.number);
								xml.attribute("active", session.active());
								xml.attribute("volume", (int)vol);
								xml.attribute("muted",  session.muted);
							});
						});
					});
				});
			} catch (...) { Genode::warning("could report current channels"); }
		}

		/*
		 * Check if any of the available session is currently active, i.e.,
		 * playing.
		 */
		bool _check_active()
		{
			bool active = false;
			_for_each_channel([&] (Channel::Number ch, Session_channel *sc) {
				sc->for_each_session([&] (Session_elem const &session) {
					active |= session.active();
				});
			});
			return active;
		}

		/*
		 * Advance the stream of the session to a new position
		 */
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

		/*
		 * Advance the position of each session to match the output position
		 */
		void _advance_position()
		{
			for_each_index(MAX_CHANNELS, [&] (int i) {
				unsigned const pos = _out[i]->stream()->pos();
				_channels[i].for_each_session([&] (Session_elem &session) {
					_advance_session(&session, pos);
				});
			});
		}

		/*
		 * Mix input packet into output packet
		 *
		 * Packets are mixed in a linear way with min/max clipping.
		 */
		void _mix_packet(Packet *out, Packet *in, bool clear,
		                 float const out_vol, float const vol)
		{
			if (clear) {
				for_each_index(Audio_out::PERIOD, [&] (int const i) {
					out->content()[i]  = (in->content()[i] * vol);

					if (out->content()[i] > 1) out->content()[i] = 1;
					if (out->content()[i] < -1) out->content()[i] = -1;

					out->content()[i] *= out_vol;
				});
			} else {
				for_each_index(Audio_out::PERIOD, [&] (int const i) {
					out->content()[i] += (in->content()[i] * vol);

					if (out->content()[i] > 1) out->content()[i] = 1;
					if (out->content()[i] < -1) out->content()[i] = -1;

					out->content()[i] *= out_vol;
				});
			}

			/* mark the packet as processed by invalidating it */
			in->invalidate();
		}

		/*
		 * Mix all session of one channel
		 */
		bool _mix_channel(bool remix, Channel::Number nr, unsigned out_pos, unsigned offset)
		{
			Stream  * const    stream  = _out[nr]->stream();
			Packet  * const    out     = stream->get(out_pos + offset);
			Session_channel * const sc = &_channels[nr];

			float const out_vol  = _out_volume[nr];

			bool       clear     = true;
			bool       mix_all   = remix;
			bool const out_valid = out->valid();

			Genode::retry<Remix_all>(
				/*
				 * Mix the input packet at the given position of every input
				 * session to one output packet.
				 */
				[&] {
					sc->for_each_session([&] (Session_elem &session) {
						if (session.stopped() || session.muted) return;

						Packet *in = session.get_packet(offset);

						/* remix again if input has changed for already mixed packet */
						if (in->valid() && out_valid && !mix_all) throw Remix_all();

						/* skip if packet has been processed or was already played */
						if ((!in->valid() && !mix_all) || in->played()) return;

						_mix_packet(out, in, clear, out_vol, session.volume);

						clear = false;
					});
				},
				/*
				 * An input packet of an already mixed output packet has
				 * changed, we have to remix all input packets again.
				 */
				[&] {
					clear   = true;
					mix_all = true;
				});

			return !clear;
		}

		/*
		 * Mix input packets
		 *
		 * \param remix force remix of already mixed packets
		 */
		void _mix(bool remix = false)
		{
			unsigned pos[MAX_CHANNELS];
			pos[LEFT]  = _out[LEFT]->stream()->pos();
			pos[RIGHT] = _out[RIGHT]->stream()->pos();

			/*
			 * Look for packets that are valid and mix channels in an alternating
			 * way.
			 */
			for_each_index(Audio_out::QUEUE_SIZE, [&] (int const i) {
				bool mix_one = true;
				for_each_index(MAX_CHANNELS, [&] (int const j) {
					mix_one = _mix_channel(remix, (Channel::Number)j, pos[j], i);
				});

				/* all channels mixed, submit to output queue */
				if (mix_one)
					for_each_index(MAX_CHANNELS, [&] (int const j) {
						Packet *p = _out[j]->stream()->get(pos[j] + i);
						_out[j]->submit(p);
					});
			});
		}

		/**
		 * Handle progress signals from Audio_out session and data available signals
		 * from each mixer client
		 */
		void _handle()
		{
			_advance_position();
			_mix();
		}

		/**
		 * Set default values for various options
		 */
		void _set_default_config(Genode::Xml_node const &node)
		{
			try {
				Genode::Xml_node default_node = node.sub_node("default");
				long v = 0;

				v = default_node.attribute_value<long>("out_volume", 0);
				_default_out_volume = (float)v / MAX_VOLUME;

				v = default_node.attribute_value<long>("volume", 0);
				_default_volume = (float)v / MAX_VOLUME;

				v = default_node.attribute_value<long>("muted", 1);
				_default_muted = v ;

				logv("default settings: "
				     "out_volume: ", (int)(MAX_VOLUME*_default_out_volume), " "
				     "volume: ",     (int)(MAX_VOLUME*_default_volume), " "
				     "muted: ",      _default_muted);

			} catch (...) { Genode::warning("could not read mixer default values"); }
		}

		/**
		 * Handle ROM update signals
		 */
		void _handle_config_update()
		{
			using namespace Genode;

			_config_rom.update();

			Xml_node config_node = _config_rom.xml();
			verbose = config_node.attribute_value("verbose", verbose);

			_set_default_config(config_node);

			try {
				Xml_node channel_list_node = config_node.sub_node("channel_list");

				channel_list_node.for_each_sub_node([&] (Xml_node const &node) {
					Channel ch(node);

					if (ch.type == Channel::Type::INPUT) {
						_for_each_channel([&] (Channel::Number, Session_channel *sc) {
							sc->for_each_session([&] (Session_elem &session) {
								if (session.number != ch.number) return;
								if (session.label != ch.label) return;

								session.volume = (float)ch.volume / MAX_VOLUME;
								session.muted  = ch.muted;

								logv("label: '", ch.label, "' "
								     "nr: ",     (int)ch.number, " "
								     "vol: ",    (int)(MAX_VOLUME*session.volume), " "
								     "muted: ",  ch.muted);
							});
						});
					}
					else if (ch.type == Channel::Type::OUTPUT) {
						for_each_index(MAX_CHANNELS, [&] (int const i) {
							if (ch.number != i) return;

							_out_volume[i] = (float)ch.volume / MAX_VOLUME;

							logv("label: 'master' "
							     "nr: ",    (int)ch.number, " "
							     "vol: ",   (int)(MAX_VOLUME*_out_volume[i]), " "
							     "muted: ", ch.muted);
						});
					}
				});
			} catch (...) { Genode::warning("mixer channel_list was invalid"); }

			/*
			 * Report back any changes so a front-end can update its state
			 */
			_report_channels();

			/*
			 * The configuration has changed, remix already mixed packets
			 * in the mixer output queue.
			 */
			_mix(true);
		}

		/*
		 * Signal handlers
		 */
		Genode::Signal_handler<Audio_out::Mixer> _handler
			{ env.ep(), *this, &Audio_out::Mixer::_handle };

		Genode::Signal_handler<Audio_out::Mixer> _handler_config
			{ env.ep(), *this, &Audio_out::Mixer::_handle_config_update  };

	public:

		/**
		 * Constructor
		 */
		Mixer(Genode::Env &env) : env(env)
		{
			_out[LEFT]  = &_left;
			_out[RIGHT] = &_right;

			_out_volume[LEFT]  = _default_out_volume;
			_out_volume[RIGHT] = _default_out_volume;

			_config_rom.sigh(_handler_config);
			_handle_config_update();

			_report_channels();
		}

		/**
		 * Start output stream
		 */
		void start()
		{
			_out[LEFT]->progress_sigh(_handler);
			for_each_index(MAX_CHANNELS, [&] (int const i) { _out[i]->start(); });
		}

		/**
		 * Stop output stream
		 */
		void stop()
		{
			for_each_index(MAX_CHANNELS, [&] (int const i) { _out[i]->stop(); });
			_out[LEFT]->progress_sigh(Genode::Signal_context_capability());
		}

		/**
		 * Get current playback position of output stream
		 */
		unsigned pos(Channel::Number channel) const { return _out[channel]->stream()->pos(); }

		/**
		 * Add input session
		 */
		void add_session(Channel::Number ch, Session_elem &session)
		{
			session.volume = _default_volume;
			session.muted  = _default_muted;

			log("add label: \"", session.label, "\" "
			    "channel: \"",   string_from_number(ch), "\" "
			    "nr: ",          (int)ch, " "
			    "volume: ",      (int)(MAX_VOLUME*session.volume), " "
			    "muted: ",     session.muted);

			_channels[ch].insert(&session);
			_report_channels();
		}

		/**
		 * Remove input session
		 */
		void remove_session(Channel::Number ch, Session_elem &session)
		{
			log("remove label: \"", session.label, "\" "
			    "channel: \"", string_from_number(ch), "\" "
			    "nr: ", (int)ch);

			_channels[ch].remove(&session);
			_report_channels();
		}

		/**
		 * Get signal context that handles data avaiable as well as progress signal
		 */
		Genode::Signal_context_capability sig_cap() { return _handler; }

		/**
		 * Report current channels
		 */
		void report_channels() { _report_channels(); }
};


/**************************************
 ** Audio_out session implementation **
 **************************************/

class Audio_out::Session_component : public Audio_out::Session_elem
{
	private:

		Mixer &_mixer;

	public:

		Session_component(Genode::Env     &env,
		                  char const      *label,
		                  Channel::Number  number,
		                  Mixer           &mixer)
		: Session_elem(env, label, mixer.sig_cap()), _mixer(mixer)
		{
			Session_elem::number = number;
			_mixer.add_session(Session_elem::number, *this);
		}

		~Session_component()
		{
			if (Session_rpc_object::active()) stop();
			_mixer.remove_session(Session_elem::number, *this);
		}

		void start()
		{
			Session_rpc_object::start();
			stream()->pos(_mixer.pos(Session_elem::number));
			_mixer.report_channels();
		}

		void stop()
		{
			Session_rpc_object::stop();
			_mixer.report_channels();
		}
};


namespace Audio_out {
	typedef Genode::Root_component<Session_component, Genode::Multiple_clients> Root_component;
}


class Audio_out::Root : public Audio_out::Root_component
{
	private:

		Genode::Env &_env;
		Mixer       &_mixer;
		int          _sessions = { 0 };

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
				Genode::error("insufficient 'ram_quota', got ", ram_quota, ", "
				              "need ", sizeof(Stream) + session_size);
				throw Root::Quota_exceeded();
			}

			Channel::Number ch = number_from_string(channel_name);
			if (ch == Channel::Number::INVALID)
				throw Root::Invalid_args();

			Session_component *session = new (md_alloc())
				Session_component(_env, label, (Channel::Number)ch, _mixer);

			if (++_sessions == 1) _mixer.start();
			return session;

		}

		void _destroy_session(Session_component *session)
		{
			if (--_sessions == 0) _mixer.stop();
			Genode::destroy(md_alloc(), session);
		}

	public:

		Root(Genode::Env        &env,
		     Mixer              &mixer,
		     Genode::Allocator  &md_alloc)
		: Root_component(env.ep(), md_alloc), _env(env), _mixer(mixer) { }
};


/***************
 ** Component **
 ***************/

namespace Mixer { struct Main; }

struct Mixer::Main
{
	Genode::Env &env;

	Genode::Sliced_heap heap { env.ram(), env.rm() };

	Audio_out::Mixer mixer = { env };
	Audio_out::Root  root  = { env, mixer, heap };

	Main(Genode::Env &env) : env(env)
	{
		env.parent().announce(env.ep().manage(root));
	}
};


void Component::construct(Genode::Env &env) { static Mixer::Main inst(env); }
