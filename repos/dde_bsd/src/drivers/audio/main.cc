/*
 * \brief  Startup audio driver library
 * \author Josef Soentgen
 * \date   2014-11-09
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */


/* Genode includes */
#include <audio_in_session/rpc_object.h>
#include <audio_out_session/rpc_object.h>
#include <base/attached_rom_dataspace.h>
#include <base/session_label.h>
#include <base/component.h>
#include <base/heap.h>
#include <base/log.h>
#include <root/component.h>

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

		Session_component(Genode::Env &env, Channel_number channel, Signal_context_capability cap)
		:
			Session_rpc_object(env, cap), _channel(channel)
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

		Genode::Env                            &_env;
		Genode::Signal_handler<Audio_out::Out>  _data_avail_dispatcher;
		Genode::Signal_handler<Audio_out::Out>  _notify_dispatcher;

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
			if (err && err != 35) {
				Genode::warning("Error ", err, " during silence playback");
			}
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

				for (unsigned i = 0; i < Audio_out::PERIOD * Audio_out::MAX_CHANNELS; i += 2) {
					data[i] = p_left->content()[i / 2] * 32767;
					data[i + 1] = p_right->content()[i / 2] * 32767;
				}

				/* send to driver */
				if (int err = Audio::play(data, sizeof(data))) {
					Genode::warning("Error ", err, " during playback");
				}

				p_left->invalidate();
				p_right->invalidate();

				p_left->mark_as_played();
				p_right->mark_as_played();

			} else {
				_play_silence();
				return;
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
		void _handle_data_avail() { }

		/*
		 * DMA block played
		 */
		void _handle_notify()
		{
			if (_active())
				_play_packet();
		}

	public:

		Out(Genode::Env &env)
		:
			_env(env),
			_data_avail_dispatcher(env.ep(), *this, &Audio_out::Out::_handle_data_avail),
			_notify_dispatcher(env.ep(), *this, &Audio_out::Out::_handle_notify)
		{
			/* play a silence packets to get the driver running */
			// XXX replace by explicit call to audio_start
			_play_silence();
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

		if (sizeof(Stream) > ram_quota) {
			Genode::error("insufficient 'ram_quota', got ", ram_quota,
			              " need ", sizeof(Stream));
			throw Genode::Insufficient_ram_quota();
		}

		char channel_name[16];
		Channel_number channel_number;
		Arg_string::find_arg(args, "channel").string(channel_name,
		                                             sizeof(channel_name),
		                                             "left");
		if (!Out::channel_number(channel_name, &channel_number)) {
			Genode::error("invalid output channel '",(char const *)channel_name,"' requested, "
			              "denying '",Genode::label_from_args(args),"'");
			throw Genode::Service_denied();
		}
		if (Audio_out::channel_acquired[channel_number]) {
			Genode::error("output channel '",(char const *)channel_name,"' is unavailable, "
			              "denying '",Genode::label_from_args(args),"'");
			throw Genode::Service_denied();
		}
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

		Genode::Env &_env;

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
				Session_component(_env, channel_number, _cap);
		}

	public:

		Root(Genode::Env &env, Allocator &md_alloc,
		     Signal_context_capability cap)
		:
			Root_component(env.ep(), md_alloc),
			_env(env), _cap(cap)
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

		Session_component(Genode::Env &env, Channel_number channel,
		                  Genode::Signal_context_capability cap)
		: Session_rpc_object(env, cap), _channel(channel) {
			channel_acquired = this; }

		~Session_component() { channel_acquired = nullptr; }
};


class Audio_in::In
{
	private:

		Genode::Env                          &_env;
		Genode::Signal_handler<Audio_in::In>  _notify_dispatcher;

		bool _active() { return channel_acquired && channel_acquired->active(); }

		Stream *stream() { return channel_acquired->stream(); }

		void _record_packet()
		{
			static short data[2 * Audio_in::PERIOD];
			if (int err = Audio::record(data, sizeof(data))) {
					if (err && err != 35) {
						Genode::warning("Error ", err, " during recording");
					}
					return;
			}

			/*
			 * Check for an overrun first and notify the client later.
			 */
			bool overrun = stream()->overrun();

			Packet *p = stream()->alloc();

			float const scale = 32768.0f * 2;

			float * const content = p->content();
			for (unsigned long i = 0; i < 2*Audio_in::PERIOD; i += 2) {
				float sample = data[i] + data[i+1];
				content[i/2] = sample / scale;
			}

			stream()->submit(p);

			channel_acquired->progress_submit();

			if (overrun) channel_acquired->overrun_submit();
		}

		void _handle_notify()
		{
			if (_active())
				_record_packet();
		}

	public:

		In(Genode::Env &env)
		:
			_env(env),
			_notify_dispatcher(env.ep(), *this, &Audio_in::In::_handle_notify)
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

		if (sizeof(Stream) > ram_quota) {
			Genode::error("insufficient 'ram_quota', got ", ram_quota,
			              " need ", sizeof(Stream),
			              ", denying '", Genode::label_from_args(args),"'");
			throw Genode::Insufficient_ram_quota();
		}

		char channel_name[16];
		Channel_number channel_number;
		Arg_string::find_arg(args, "channel").string(channel_name,
		                                             sizeof(channel_name),
		                                             "left");
		if (!In::channel_number(channel_name, &channel_number)) {
			Genode::error("invalid input channel '",(char const *)channel_name,"' requested, "
			              "denying '",Genode::label_from_args(args),"'");
			throw Genode::Service_denied();
		}
		if (Audio_in::channel_acquired) {
			Genode::error("input channel '",(char const *)channel_name,"' is unavailable, "
			              "denying '",Genode::label_from_args(args),"'");
			throw Genode::Service_denied();
		}
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

		Genode::Env               &_env;
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
			return new (md_alloc()) Session_component(_env, channel_number, _cap);
		}

	public:

		Root(Genode::Env &env, Allocator &md_alloc,
		     Signal_context_capability cap)
		: Root_component(env.ep(), md_alloc), _env(env), _cap(cap) { }
};


/**********
 ** Main **
 **********/

struct Main
{
	Genode::Env        &env;
	Genode::Heap       heap { &env.ram(), &env.rm() };

	Genode::Attached_rom_dataspace config { env, "config" };

	Genode::Signal_handler<Main> config_update_dispatcher {
		env.ep(), *this, &Main::handle_config_update };

	void handle_config_update()
	{
		config.update();
		if (!config.valid()) { return; }
		Audio::update_config(env, config.xml());
	}

	Genode::Constructible<Audio_out::Out>  _out      { };
	Genode::Constructible<Audio_out::Root> _out_root { };

	Genode::Constructible<Audio_in::In>   _in      { };
	Genode::Constructible<Audio_in::Root> _in_root { };

	Genode::Signal_handler<Main> announce_session_dispatcher {
		env.ep(), *this, &Main::handle_announce_session };

	void handle_announce_session()
	{
		_out.construct(env);
		Audio::play_sigh(_out->sigh());

		_out_root.construct(env, heap, _out->data_avail());
		env.parent().announce(env.ep().manage(*_out_root));

		_in.construct(env);
		Audio::record_sigh(_in->sigh());

		_in_root.construct(env, heap,
		                   Genode::Signal_context_capability());
		env.parent().announce(env.ep().manage(*_in_root));
	}

	Main(Genode::Env &env) : env(env)
	{
		config.sigh(config_update_dispatcher);

		Audio::init_driver(env, heap, config.xml(),
		                   announce_session_dispatcher);
	}
};


void Component::construct(Genode::Env &env)
{
	/* XXX execute constructors of global statics */
	env.exec_static_constructors();

	static Main server(env);
}
