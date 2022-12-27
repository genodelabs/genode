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
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/log.h>
#include <base/sleep.h>
#include <base/heap.h>
#include <root/component.h>
#include <audio_out_session/rpc_object.h>
#include <util/misc_math.h>
#include <timer_session/connection.h>

/* local includes */
#include <alsa.h>


using namespace Genode;

enum Channel_number { LEFT, RIGHT, MAX_CHANNELS, INVALID = MAX_CHANNELS };


namespace Audio_out
{
	class  Session_component;
	class  Out;
	class  Root;
	struct Root_policy;
	struct Main;

	static Session_component *channel_acquired[MAX_CHANNELS];
};


class Audio_out::Session_component : public Audio_out::Session_rpc_object
{
	private:

		Channel_number _channel;

	public:

		Session_component(Genode::Env &env, Channel_number channel, Signal_context_capability data_cap)
		:
			Session_rpc_object(env, data_cap),
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

		Genode::Env                            &_env;
		Genode::Signal_handler<Audio_out::Out>  _data_avail_dispatcher;
		Genode::Signal_handler<Audio_out::Out>  _timer_dispatcher;

		Timer::Connection _timer { _env };

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
			Packet *p_right = right()->get(left()->pos());

			/* convert float to S16LE */
			static short data[2 * PERIOD];

			if (p_left->valid() && p_right->valid()) {

				for (unsigned i = 0; i < 2 * PERIOD; i += 2) {
					data[i]     = (short)(p_left ->content()[i / 2] * 32767.0f);
					data[i + 1] = (short)(p_right->content()[i / 2] * 32767.0f);
				}

				p_left->invalidate();
				p_right->invalidate();

				/* blocking-write packet to ALSA */
				while (audio_drv_play(data, PERIOD)) {
					/* try to restart the driver silently */
					audio_drv_stop();
					audio_drv_start();
				}

				p_left->mark_as_played();
				p_right->mark_as_played();
			}

			_advance_position(p_left, p_right);

			return true;
		}

		void _handle_data_avail() { }

		void _handle_timer()
		{
			if (_active()) _play_packet();
		}

	public:

		Out(Genode::Env &env)
		:
			_env(env),
			_data_avail_dispatcher(env.ep(), *this, &Audio_out::Out::_handle_data_avail),
			_timer_dispatcher(env.ep(), *this, &Audio_out::Out::_handle_timer)
		{
			_timer.sigh(_timer_dispatcher);

			uint64_t const us = (Audio_out::PERIOD * 1000 / Audio_out::SAMPLE_RATE)*1000;
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
		if (!channel_number_from_string(channel_name, &channel_number))
			throw Genode::Service_denied();
		if (Audio_out::channel_acquired[channel_number])
			throw Genode::Service_denied();
	}

	void release() { }
};


namespace Audio_out {
	typedef Root_component<Session_component, Root_policy> Root_component;
}


class Audio_out::Root : public Audio_out::Root_component
{
	private:

		Genode::Env &_env;

		Signal_context_capability _data_cap;

	protected:

		Session_component *_create_session(const char *args) override
		{
			char channel_name[16];
			Channel_number channel_number = INVALID;
			Arg_string::find_arg(args, "channel").string(channel_name,
			                                             sizeof(channel_name),
			                                             "left");
			channel_number_from_string(channel_name, &channel_number);

			return new (md_alloc())
				Session_component(_env, channel_number, _data_cap);
		}

	public:

		Root(Genode::Env &env, Allocator &md_alloc,
		     Signal_context_capability data_cap)
		: Root_component(env.ep(), md_alloc), _env(env), _data_cap(data_cap)
		{ }
};


struct Audio_out::Main
{
	Genode::Env  &env;
	Genode::Heap  heap { env.ram(), env.rm() };

	Genode::Attached_rom_dataspace config { env, "config" };

	Main(Genode::Env &env) : env(env)
	{
		typedef Genode::String<32> Dev;
		Dev const dev = config.xml().attribute_value("alsa_device", Dev("hw"));

		/* init ALSA */
		int err = audio_drv_init(dev.string());
		if (err) {
			if (err == -1) {
				Genode::error("could not open ALSA device ", dev);
			} else {
				Genode::error("could not initialize driver error ", err);
			}

			throw -1;
		}
		audio_drv_start();

		static Audio_out::Out  out(env);
		static Audio_out::Root root(env, heap, out.data_avail_sigh());
		env.parent().announce(env.ep().manage(root));
		Genode::log("--- start Audio_out ALSA driver ---");
	}
};


/***************
 ** Component **
 ***************/

void Component::construct(Genode::Env &env) { static Audio_out::Main main(env); }
