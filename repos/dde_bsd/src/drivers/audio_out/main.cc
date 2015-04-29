/*
 * \brief  Startup audio driver library
 * \author Josef Soentgen
 * \date   2014-11-09
 */

#include <audio_out_session/rpc_object.h>
#include <base/env.h>
#include <base/sleep.h>
#include <cap_session/connection.h>
#include <os/server.h>
#include <root/component.h>
#include <util/misc_math.h>

#include <audio/audio.h>

static bool const verbose    = false;

using namespace Genode;

using namespace Audio;

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

		Channel_number      _channel;

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
		Genode::Signal_rpc_member<Audio_out::Out>  _dma_notify_dispatcher;

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

		void _play_silence()
		{
			static short silence[2 * Audio_out::PERIOD] = { 0 };

			if (int err = Audio::play(silence, sizeof(silence)))
				PWRN("Error %d during silence playback", err);
		}

		void _play_packet()
		{
			Packet *p_left  = left()->get(left()->pos());
			Packet *p_right = right()->get(right()->pos());

			if ((p_left->valid() && p_right->valid())) {
				/* convert float to S16LE */
				static short data[2 * Audio_out::PERIOD];

				for (int i = 0; i < 2 * Audio_out::PERIOD; i += 2) {
					data[i] = p_left->content()[i / 2] * 32767;
					data[i + 1] = p_right->content()[i / 2] * 32767;
				}

				/* send to driver */
				if (int err = Audio::play(data, sizeof(data)))
					PWRN("Error %d during playback", err);
			} else
				_play_silence();

			p_left->invalidate();
			p_right->invalidate();

			p_left->mark_as_played();
			p_right->mark_as_played();

			_advance_position(p_left, p_right);
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
		 *
		 * After each block played from the DMA buffer, the hw will
		 * generated an interrupt. The IRQ handling code will notify
		 * us as soon as this happens and we will play the next
		 * packet.
		 */
		void _handle_dma_notify(unsigned)
		{
			if (!_active())
				return;

			_play_packet();
		}

	public:

		Out(Server::Entrypoint &ep)
		:
			_ep(ep),
			_data_avail_dispatcher(ep, *this, &Audio_out::Out::_handle_data_avail),
			_dma_notify_dispatcher(ep, *this, &Audio_out::Out::_handle_dma_notify)
		{
			/* play a silence packet to get the driver running */
			_play_silence();
		}

		Signal_context_capability data_avail() { return _data_avail_dispatcher; }

		Signal_context_capability dma_notifier() { return _dma_notify_dispatcher; }

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
			channel_number_from_string(channel_name, &channel_number);

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


struct Main
{
	Server::Entrypoint &ep;

	Main(Server::Entrypoint &ep) : ep(ep)
	{
		Audio::init_driver(ep);

		if (Audio::driver_active()) {
			static Audio_out::Out out(ep);
			Audio::dma_notifier(out.dma_notifier());
			static Audio_out::Root audio_root(ep, *env()->heap(), out.data_avail());

			PINF("--- BSD Audio_out driver started ---");
			env()->parent()->announce(ep.manage(audio_root));
		}
	}
};


namespace Server {
	char const *name()             { return "audio_drv_ep";      }
	size_t      stack_size()       { return 4*1024*sizeof(long); }
	void construct(Entrypoint &ep) { static Main server(ep);     }
}
