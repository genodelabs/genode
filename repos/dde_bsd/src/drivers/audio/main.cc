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
#include <record_session/connection.h>
#include <play_session/connection.h>
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
	using Root_component = Root_component<Session_component, Root_policy>;

	static Session_component *channel_acquired[MAX_CHANNELS];
}


class Audio_out::Session_component : public Audio_out::Session_rpc_object
{
	private:

		Channel_number _channel;

	public:

		Session_component(Env &env, Channel_number channel, Signal_context_capability cap)
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

		Env                            &_env;
		Signal_handler<Audio_out::Out>  _data_avail_handler;
		Signal_handler<Audio_out::Out>  _notify_handler;

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
				warning("error ", err, " during silence playback");
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
					warning("error ", err, " during playback");
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

		Out(Env &env)
		:
			_env(env),
			_data_avail_handler(env.ep(), *this, &Audio_out::Out::_handle_data_avail),
			_notify_handler(env.ep(), *this, &Audio_out::Out::_handle_notify)
		{
			/* play a silence packets to get the driver running */
			// XXX replace by explicit call to audio_start
			_play_silence();
			_play_silence();
		}

		Signal_context_capability data_avail() { return _data_avail_handler; }

		Signal_context_capability sigh() { return _notify_handler; }

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
				if (!strcmp(name, n->name)) {
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
			error("insufficient 'ram_quota', got ", ram_quota,
			      " need ", sizeof(Stream));
			throw Insufficient_ram_quota();
		}

		char channel_name[16];
		Channel_number channel_number;
		Arg_string::find_arg(args, "channel").string(channel_name,
		                                             sizeof(channel_name),
		                                             "left");
		if (!Out::channel_number(channel_name, &channel_number)) {
			error("invalid output channel '",(char const *)channel_name,"' requested, "
			      "denying '", label_from_args(args), "'");
			throw Service_denied();
		}
		if (Audio_out::channel_acquired[channel_number]) {
			error("output channel '",(char const *)channel_name,"' is unavailable, "
			      "denying '", label_from_args(args), "'");
			throw Service_denied();
		}
	}

	void release() { }
};


/**
 * Root component, handling new session requests.
 */
class Audio_out::Root : public Audio_out::Root_component
{
	private:

		Env &_env;

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

		Root(Env &env, Allocator &md_alloc,
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
	using Root_component = Root_component<Session_component, Root_policy>;

	static Session_component *channel_acquired;
}


class Audio_in::Session_component : public Audio_in::Session_rpc_object
{
	private:

		Channel_number _channel;

	public:

		Session_component(Env &env, Channel_number channel,
		                  Signal_context_capability cap)
		: Session_rpc_object(env, cap), _channel(channel) {
			channel_acquired = this; }

		~Session_component() { channel_acquired = nullptr; }
};


class Audio_in::In
{
	private:

		Env                          &_env;
		Signal_handler<Audio_in::In>  _notify_handler;

		bool _active() { return channel_acquired && channel_acquired->active(); }

		Stream *stream() { return channel_acquired->stream(); }

		void _record_packet()
		{
			static short data[2 * Audio_in::PERIOD];
			if (int err = Audio::record(data, sizeof(data))) {
					if (err && err != 35) {
						warning("error ", err, " during recording");
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

		In(Env &env)
		:
			_env(env),
			_notify_handler(env.ep(), *this, &Audio_in::In::_handle_notify)
		{ _record_packet(); }

		Signal_context_capability sigh() { return _notify_handler; }

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
				if (!strcmp(name, n->name)) {
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
			error("insufficient 'ram_quota', got ", ram_quota,
			      " need ", sizeof(Stream),
			      ", denying '", label_from_args(args),"'");
			throw Insufficient_ram_quota();
		}

		char channel_name[16];
		Channel_number channel_number;
		Arg_string::find_arg(args, "channel").string(channel_name,
		                                             sizeof(channel_name),
		                                             "left");
		if (!In::channel_number(channel_name, &channel_number)) {
			error("invalid input channel '",(char const *)channel_name,"' requested, "
			      "denying '", label_from_args(args),"'");
			throw Service_denied();
		}
		if (Audio_in::channel_acquired) {
			error("input channel '",(char const *)channel_name,"' is unavailable, "
			      "denying '", label_from_args(args),"'");
			throw Service_denied();
		}
	}

	void release() { }
};


/**
 * Root component, handling new session requests.
 */
class Audio_in::Root : public Audio_in::Root_component
{
	private:

		Env                       &_env;
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

		Root(Env &env, Allocator &md_alloc, Signal_context_capability cap)
		: Root_component(env.ep(), md_alloc), _env(env), _cap(cap) { }
};


struct Stereo_output : Noncopyable
{
	static constexpr unsigned SAMPLES_PER_PERIOD = Audio_in::PERIOD;
	static constexpr unsigned CHANNELS = 2;

	Env &_env;

	Record::Connection _left  { _env, "left"  };
	Record::Connection _right { _env, "right" };

	struct Recording : private Noncopyable
	{
		bool depleted = false;

		/* 16 bit per sample, interleaved left and right */
		int16_t data[SAMPLES_PER_PERIOD*CHANNELS] { };

		void clear() { for (auto &e : data) e = 0; }

		void from_record_sessions(Record::Connection &left, Record::Connection &right)
		{
			using Samples_ptr = Record::Connection::Samples_ptr;

			Record::Num_samples const num_samples { SAMPLES_PER_PERIOD };

			auto clamped = [&] (float v)
			{
				return (v >  1.0) ?  1.0
				     : (v < -1.0) ? -1.0
				     :  v;
			};

			auto float_to_s16 = [&] (float v) { return int16_t(clamped(v)*32767); };

			left.record(num_samples,
				[&] (Record::Time_window const tw, Samples_ptr const &samples) {
					depleted = false;

					for (unsigned i = 0; i < SAMPLES_PER_PERIOD; i++)
						data[i*CHANNELS] = float_to_s16(samples.start[i]);

					right.record_at(tw, num_samples,
						[&] (Samples_ptr const &samples) {
							for (unsigned i = 0; i < SAMPLES_PER_PERIOD; i++)
								data[i*CHANNELS + 1] = float_to_s16(samples.start[i]);
						});
				},
				[&] {
					depleted = true;
					clear();
				}
			);
		}
	};

	Recording _recording { };

	Signal_handler<Stereo_output> _output_handler {
		_env.ep(), *this, &Stereo_output::_handle_output };

	void _handle_output()
	{
		_recording.from_record_sessions(_left, _right);
		Audio::play(_recording.data, sizeof(_recording.data));
	}

	Stereo_output(Env &env) : _env(env)
	{
		Audio::play_sigh(_output_handler);

		/* submit two silent packets to get the driver going */
		Audio::play(_recording.data, sizeof(_recording.data));
		Audio::play(_recording.data, sizeof(_recording.data));
	}
};


struct Stereo_input : Noncopyable
{
	static constexpr unsigned SAMPLES_PER_PERIOD = Audio_in::PERIOD;
	static constexpr unsigned CHANNELS = 2;

	Env &_env;

	Play::Connection _left  { _env, "left"  };
	Play::Connection _right { _env, "right" };

	/* 16 bit per sample, interleaved left and right */
	int16_t data[SAMPLES_PER_PERIOD*CHANNELS] { };

	struct Frame { float left, right; };

	void _for_each_frame(auto const &fn) const
	{
		float const scale = 1.0f/32768;

		for (unsigned i = 0; i < SAMPLES_PER_PERIOD; i++)
			fn(Frame { .left  = scale*float(data[i*CHANNELS]),
			           .right = scale*float(data[i*CHANNELS + 1]) });
	}

	Play::Time_window _time_window { };

	Signal_handler<Stereo_input> _input_handler {
		_env.ep(), *this, &Stereo_input::_handle_input };

	void _handle_input()
	{
		if (int const err = Audio::record(data, sizeof(data))) {
			if (err && err != 35)
				warning("error ", err, " during recording");
			return;
		}

		Play::Duration const duration_us { 11*1000 }; /* hint for first period */
		_time_window = _left.schedule_and_enqueue(_time_window, duration_us,
			[&] (auto &submit) {
				_for_each_frame([&] (Frame const frame) {
					submit(frame.left); }); });

		_right.enqueue(_time_window,
			[&] (auto &submit) {
				_for_each_frame([&] (Frame const frame) {
					submit(frame.right); }); });
	}

	Stereo_input(Env &env) : _env(env)
	{
		Audio::record_sigh(_input_handler);
	}
};


/**********
 ** Main **
 **********/

struct Main
{
	Env  &_env;
	Heap  _heap { _env.ram(), _env.rm() };

	Attached_rom_dataspace _config { _env, "config" };

	Signal_handler<Main> _config_update_handler {
		_env.ep(), *this, &Main::_handle_config_update };

	void _handle_config_update()
	{
		_config.update();
		Audio::update_config(_env, _config.xml());
	}

	bool const _record_play = _config.xml().attribute_value("record_play", false);

	Constructible<Audio_out::Out>  _out      { };
	Constructible<Audio_out::Root> _out_root { };

	Constructible<Audio_in::In>   _in      { };
	Constructible<Audio_in::Root> _in_root { };

	Constructible<Stereo_output> _stereo_output { };
	Constructible<Stereo_input>  _stereo_input  { };

	Signal_handler<Main> _announce_session_handler {
		_env.ep(), *this, &Main::_handle_announce_session };

	void _handle_announce_session()
	{
		if (_record_play) {
			_stereo_output.construct(_env);
			_stereo_input .construct(_env);
			return;
		}

		/* Audio_out/Audio_in mode */

		_out.construct(_env);
		Audio::play_sigh(_out->sigh());

		_out_root.construct(_env, _heap, _out->data_avail());
		_env.parent().announce(_env.ep().manage(*_out_root));

		_in.construct(_env);
		Audio::record_sigh(_in->sigh());

		_in_root.construct(_env, _heap, Signal_context_capability());
		_env.parent().announce(_env.ep().manage(*_in_root));
	}

	Main(Env &env) : _env(env)
	{
		_config.sigh(_config_update_handler);

		Audio::init_driver(_env, _heap, _config.xml(), _announce_session_handler);
	}
};


void Component::construct(Genode::Env &env)
{
	/* XXX execute constructors of global statics */
	env.exec_static_constructors();

	static Main server(env);
}
