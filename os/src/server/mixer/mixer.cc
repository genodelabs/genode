/*
 * \brief  Audio Mixer
 * \author Sebastian Sumpf
 * \author Christian Helmuth
 * \author Stefan Kalkowski
 * \date   2009-12-02
 *
 * The mixer supports up to MAX_TRACKS (virtual) two-channel stereo input
 * sessions and, therefore, provides audio-out sessions for "front left" and
 * "front right". The mixer itself uses two audio-out sessions - front left and
 * right.
 */

/*
 * Copyright (C) 2009-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/env.h>
#include <base/sleep.h>
#include <base/semaphore.h>
#include <base/allocator_avl.h>
#include <util/misc_math.h>

#include <root/component.h>
#include <cap_session/connection.h>
#include <audio_out_session/rpc_object.h>
#include <audio_out_session/connection.h>

using namespace Genode;

static const bool verbose = false;
static bool audio_out_active = false;
static Genode::Lock session_lock;

static Rpc_entrypoint *audio_out_ep()
{
	static Cap_connection cap;

	enum { STACK_SIZE = 4096 };
	static Rpc_entrypoint _audio_out_ep(&cap, STACK_SIZE, "audio_ep");

	return &_audio_out_ep;
}


namespace Audio_out {

	enum { OUT_QUEUE_SIZE = 1 };

	template <typename LT>
	class Ring
	{
		private:

			LT *_next;
			LT *_prev;

		public:

			Ring()
			: _next(static_cast<LT *>(this)),
			  _prev(static_cast<LT *>(this)) { }

			LT *next() { return _next; };
			LT *prev() { return _prev; };

			/**
			 * Conflate with another ring.
			 */
			bool conflate(LT *le)
			{
				/* test whether the given ring is part of this one */
				LT *e = static_cast<LT *>(this);
				while (e->Ring::_next != this) {
					if (e->Ring::_next == le)
						return false;
					e = e->Ring::_next;
				}

				/* wire this->next with le->prev */
				_next->Ring::_prev = le->Ring::_prev;
				le->Ring::_prev->Ring::_next = _next;
				_next = le;
				le->Ring::_prev = static_cast<LT *>(this);
				return true;
			}

			/**
			 * Remove this object from the ring.
			 */
			void remove()
			{
				_prev->Ring::_next = _next;
				_next->Ring::_prev = _prev;
				_next = static_cast<LT *>(this);
				_prev = static_cast<LT *>(this);
			}
	};

	enum Channel_number { LEFT, RIGHT, MAX_CHANNELS, INVALID = MAX_CHANNELS };

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

	static const char * channel_string_from_number(Channel_number number)
	{
		static const char *names[MAX_CHANNELS + 1] = {
			"front left", "front right", "invalid"
		};

		return names[number];
	}

	static Semaphore open_tracks;
	static int       num_open_tracks[MAX_CHANNELS];

	/*
	 * The mixer uses only one signal receiver and context for all input
	 * tracks.
	 */
	static Signal_receiver           avail_recv;
	static Signal_context            avail_ctx;
	static Signal_context_capability avail_cap(avail_recv.manage(&avail_ctx));

	enum { MAX_TRACKS = 16 };


	class Communication_buffer : Ram_dataspace_capability
	{
		public:

			Communication_buffer(size_t size)
			: Ram_dataspace_capability(env()->ram_session()->alloc(size))
			{ }

			~Communication_buffer() { env()->ram_session()->free(*this); }

			Dataspace_capability dataspace() { return *this; }
	};


	class Session_component : public  List<Session_component>::Element,
	                          public  Ring<Session_component>,
	                          private Communication_buffer,
	                          public  Session_rpc_object
	{
		private:

			Channel_number _channel;

		public:

			Session_component(Channel_number channel, size_t buffer_size,
			                  Rpc_entrypoint &ep)
			:
				Communication_buffer(buffer_size),
				Session_rpc_object(Communication_buffer::dataspace(), ep),
				_channel(channel)
			{
				if (verbose) PDBG("new session %p", this);

				track_list()->insert(this);
				num_open_tracks[_channel]++;

				open_tracks.up();
			}

			~Session_component()
			{
				Genode::Lock::Guard lock_guard(session_lock);
				open_tracks.down();

				num_open_tracks[_channel]--;
				track_list()->remove(this);
				Ring<Session_component>::remove();

				if (verbose) PDBG("session %p closed", this);
			}

			static List<Session_component> *track_list()
			{
				static List<Session_component> _track_list;
				return &_track_list;
			}

			/*
			 * We only need one central signal context within the mixer.
			 */
			Signal_context_capability _sigh_packet_avail()
			{
				return avail_cap;
			}

			Channel_number channel_number() const { return _channel; }

			bool all_channel_packet_avail() {
				if (!channel()->packet_avail())
					return false;

				Session_component *s = this;
				while (s->Ring<Session_component>::next() != this) {
					s = s->Ring<Session_component>::next();
					if (!s->channel()->packet_avail())
						return false;
				}
				return true;
			}

			void flush()
			{
				while (channel()->packet_avail())
					channel()->acknowledge_packet(channel()->get_packet());
			}

			void sync_session(Session_capability audio_out_session)
			{
				Session_component *sc = dynamic_cast<Session_component *>
					(audio_out_ep()->obj_by_cap(audio_out_session));

				/* check if recipient is a valid session component */
				if (!sc) return;

				if (this->conflate(sc))
					track_list()->remove(this);
			}
	};

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
			if (!(num_open_tracks[channel_number] < MAX_TRACKS)) {
				PERR("throw");
				throw Root::Unavailable();
			}
		}

		void release() { }
	};

	typedef Root_component<Session_component, Root_policy> Root_component;

	class Root : public Root_component
	{
		private:

			Rpc_entrypoint &_channel_ep;

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

				Session_component *session = new (md_alloc())
				                             Session_component(channel_number, buffer_size,
				                                               _channel_ep);

				PDBG("Added new \"%s\" channel %d/%d",
				     channel_name, num_open_tracks[channel_number], MAX_TRACKS);

				return session;
			}

			public:

				Root(Rpc_entrypoint *session_ep, Allocator *md_alloc)
				: Root_component(session_ep, md_alloc), _channel_ep(*session_ep) { }
	};

	class Packet_cache
	{
		private:

			class Track_packet;
			class Channel_packet;


			typedef Tslab<Track_packet,   1024> Track_packet_slab;
			typedef Tslab<Channel_packet, 1024> Channel_packet_slab;

			Track_packet_slab   _track_packet_slab;
			Channel_packet_slab _channel_packet_slab;

			Genode::Lock        _lock;


			class Track_packet : public List<Track_packet>::Element
			{
				private:

					Packet_descriptor       _packet;
					Session::Channel::Sink *_sink;

				public:

					Track_packet(Packet_descriptor p,
					             Session::Channel::Sink *sink)
					: _packet(p), _sink(sink) {}


					bool session_active()
					{
						for (Session_component *session = Session_component::track_list()->first();
						     session;
									session = session->List<Session_component>::Element::next())

							for (Session_component *sc = session;
							     true;
							     sc = sc->Ring<Session_component>::next()) {
								if (sc->channel() == _sink)
									return true;

								if (sc->Ring<Session_component>::next() == session)
									break;
							}

							return false;
					}

					void acknowledge() {
						if (session_active())
							_sink->acknowledge_packet(_packet); }
			};


			class Channel_packet : public List<Channel_packet>::Element
			{
				private:

					List<Track_packet> _track_packets;
					Packet_descriptor  _packet;
					Track_packet_slab *_slab;

				public:

					Channel_packet(Packet_descriptor p, Track_packet_slab *slab)
					: _packet(p), _slab(slab) { }

					void add(Track_packet *p) { _track_packets.insert(p); }

					void acknowledge()
					{
						Track_packet *p = _track_packets.first();
						while (p) {
							p->acknowledge();
							_track_packets.remove(p);
							_slab->free(p);
							p = _track_packets.first();
						}
					}

					bool packet(Packet_descriptor p)
					{
						return p.size() == _packet.size() &&
						       p.offset() == _packet.offset();
					}
			};


			List<Channel_packet> _channel_packets[MAX_CHANNELS];
			Connection          *_out_stream[MAX_CHANNELS];

		public:

			Packet_cache(Connection *output_stream[MAX_CHANNELS])
			: _track_packet_slab(Genode::env()->heap()),
			  _channel_packet_slab(Genode::env()->heap())
			{
				for (int chn = 0; chn < MAX_CHANNELS; ++chn)
					_out_stream[chn] = output_stream[chn];
			}

			void ack_packets()
			{
				for (int chn = 0; chn < MAX_CHANNELS; ++chn) {
					Session::Channel::Source *stream = _out_stream[chn]->stream();

					Packet_descriptor p = stream->get_acked_packet();

					if (verbose) PDBG("ack channel %d", chn);

					Genode::Lock::Guard lock_guard(_lock);
					Channel_packet *ch_packet = _channel_packets[chn].first();
					while (ch_packet) {
						if (ch_packet->packet(p)) {
							ch_packet->acknowledge();
							_channel_packets[chn].remove(ch_packet);
							_channel_packet_slab.free(ch_packet);
							break;
						}
						ch_packet = ch_packet->next();
					}
					stream->release_packet(p);
				}
			}

			void put(Packet_descriptor p, Session::Channel::Sink **sinks,
			         Packet_descriptor *cli_p, int count, int chn)
			{
				Genode::Lock::Guard lock_guard(_lock);

				Channel_packet *ch_packet = new (&_channel_packet_slab)
				                            Channel_packet(p, &_track_packet_slab);
				for (int i = 0; i < count; ++i) {
					Track_packet *t_packet = new (&_track_packet_slab)
					                         Track_packet(cli_p[i], sinks[i]);
					ch_packet->add(t_packet);
				}
				_channel_packets[chn].insert(ch_packet);
			}
	};

	class Mixer : private Genode::Thread<4096>
	{
		private:

			struct Mixer_packets
			{
				Session::Channel::Sink *sink[MAX_TRACKS];
				Packet_descriptor       packet[MAX_TRACKS];
				int                     count;
			} _packets[MAX_CHANNELS];

			Connection   *_out_stream[MAX_CHANNELS];
			Semaphore     _packet_sema;   /* packets available */
			Packet_cache *_cache;
			Semaphore     _startup_sema;  /* thread startup sync */

			class Receiver : private Genode::Thread<4096>
			{
				private:

					Packet_cache *_cache;
					Semaphore     _startup_sema;
					Semaphore    *_packet_sema;

					void entry()
					{
						/* indicate thread-startup completion */
						_startup_sema.up();
						while (true) {
							_cache->ack_packets();
							_packet_sema->up();
						}
					}

				public:

					Receiver(Packet_cache *cache, Semaphore *packet_sema)
					: Genode::Thread<4096>("ack"), _cache(cache),
					  _packet_sema(packet_sema)
					{
						if (audio_out_active) {
							start();
							_startup_sema.down();
						}
					}
			};

			Receiver _receiver;

			bool _get_packets()
			{
				bool packet_avail = false;

				for (int i = 0; i < MAX_CHANNELS; ++i)
					_packets[i].count = 0;

				for (Session_component *session = Session_component::track_list()->first();
				     session;
				     session = session->List<Session_component>::Element::next()) {

					if (!session->all_channel_packet_avail())
						continue;

					for (Session_component *sc = session;
					     true;
					     sc = sc->Ring<Session_component>::next()) {
						Channel_number chn = sc->channel_number();
						int            cnt = _packets[chn].count;

						_packets[chn].packet[cnt] = sc->channel()->get_packet();
						_packets[chn].sink[cnt]   = sc->channel();

						_packets[chn].count++;
						packet_avail = true;

						if (sc->Ring<Session_component>::next() == session)
							break;
					}
				}

				return packet_avail;
			}

			Packet_descriptor _mix_one_channel(Mixer_packets *packets,
			                                   Session::Channel::Source *stream)
			{
				static int alloc_cnt, alloc_fail_cnt;
				Packet_descriptor p;

				while (1)
					try {
						p = stream->alloc_packet(FRAME_SIZE * PERIOD);
						if (verbose)
							alloc_cnt++;
						break;
					} catch (Session::Channel::Source::Packet_alloc_failed) {
						PERR("Packet allocation failed %d %d",
						     ++alloc_fail_cnt, alloc_cnt);
					}

				float *out = stream->packet_content(p);
				for (int size = 0; size < PERIOD; ++size) {
					float sample = 0;
					for (int i = 0; i < packets->count; ++i) {
						sample += packets->sink[i]->packet_content(packets->packet[i])[size];
					}

					if (sample > 1) sample = 1;
					if (sample < -1) sample = -1;

					out[size] = sample;
				}

				return p;
			}

			void _mix(Packet_descriptor p[MAX_CHANNELS])
			{
				/* block if no packets are available */
				session_lock.lock();
				while (!_get_packets()) {
					session_lock.unlock();
					avail_recv.wait_for_signal();
					session_lock.lock();
				}
				_packet_sema.down();

				/* mix packets */
				for (int chn = 0; chn < MAX_CHANNELS; ++chn)
					p[chn] = _mix_one_channel(&_packets[chn],
					                          _out_stream[chn]->stream());
				session_lock.unlock();

				/* put packets into packet cache */
				for (int chn = 0; chn < MAX_CHANNELS; ++chn)
					_cache->put(p[chn], _packets[chn].sink, _packets[chn].packet,
					            _packets[chn].count, chn);
			}

			void entry()
			{
				/* indicate thread-startup completion */
				_startup_sema.up();

				/* just acknowledge packets if we don't have an audio out stream */
				while (!audio_out_active) {

					while (!_get_packets())
						avail_recv.wait_for_signal();

					for (int chn = 0; chn < MAX_CHANNELS; ++chn)
						for (int i = 0; i < _packets[chn].count; i++)
							_packets[chn].sink[i]->acknowledge_packet(_packets[chn].packet[i]);
				}

				while (1) {
					open_tracks.down();

					/* check and mix sources */
					Packet_descriptor p[MAX_CHANNELS];
					_mix(p);

					/* submit to out */
					for (int chn = 0; chn < MAX_CHANNELS; ++chn) {
						Session::Channel::Source *stream = _out_stream[chn]->stream();

						stream->submit_packet(p[chn]);
					}

					if (verbose)
						PDBG("packet submitted");

					open_tracks.up();
				}
			}

		public:

			Mixer(Connection *output_stream[MAX_CHANNELS], Packet_cache *cache)
			: Genode::Thread<4096>("tx"), _packet_sema(OUT_QUEUE_SIZE),
			  _cache(cache), _receiver(cache, &_packet_sema)
			{
				for (int chn = 0; chn < MAX_CHANNELS; ++chn)
					_out_stream[chn] = output_stream[chn];

				/* synchronize with mixer thread startup */
				start();
				_startup_sema.down();
			}
	};
}


int main(int argc, char **argv)
{
	PDBG("-- Genode Audio Mixer --");

	using namespace Audio_out;

	/* setup audio-out connections for output */
	static Audio_out::Connection *output_stream[MAX_CHANNELS];

	try {
		for (int i = 0; i < MAX_CHANNELS; ++i) {
			Allocator_avl *block_alloc = new (env()->heap()) Allocator_avl(env()->heap());
			output_stream[i] = new (env()->heap())
				Audio_out::Connection(channel_string_from_number((Channel_number)i), block_alloc, OUT_QUEUE_SIZE * FRAME_SIZE * PERIOD + 0x400);
		}
		audio_out_active = true;
	} catch (...) {
		PWRN("no audio driver found - dropping incoming packets");
	}

	/* initialize packet cache */
	static Packet_cache cache(output_stream);

	/* setup service */
	static Audio_out::Root mixer_root(audio_out_ep(), env()->heap());
	env()->parent()->announce(audio_out_ep()->manage(&mixer_root));

	/* start mixer */
	static Mixer mixer(output_stream, &cache);

	sleep_forever();
	return 0;
}
