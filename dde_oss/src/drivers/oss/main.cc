/*
 * \brief  Audio-session-entry point
 * \author Sebastian Sumpf
 * \date   2012-11-20
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

extern "C" {
#include <oss_config.h>
}

#include <base/env.h>
#include <base/sleep.h>
#include <root/component.h>
#include <cap_session/connection.h>
#include <audio_out_session/rpc_object.h>
#include <util/misc_math.h>

#include <audio.h>
#include <signal.h>

using namespace Genode;

static const bool verbose = false;
static bool audio_out_active = false;


namespace Audio_out {

	class Session_component;

	enum Channel_number { LEFT, RIGHT, MAX_CHANNELS, INVALID = MAX_CHANNELS };

	static Session_component *channel_acquired[MAX_CHANNELS];

	class Session_component : public Session_rpc_object
	{
		private:

			Ram_dataspace_capability _ds;
			Channel_number           _channel;

			Signal_dispatcher<Session_component> _process_packet_dispatcher;

			Ram_dataspace_capability _alloc_dataspace(size_t size)
			{
				_ds = env()->ram_session()->alloc(size);
				return _ds;
			}

			void _process_packets(unsigned)
			{
				/* handle audio-out packets */
				Session_component *left  = channel_acquired[LEFT],
				                  *right = channel_acquired[RIGHT];
				while (left->channel()->packet_avail() &&
				       right->channel()->packet_avail() &&
				       left->channel()->ready_to_ack()  &&
				       right->channel()->ready_to_ack()) {

					/* get packets for channels */
					Packet_descriptor p[MAX_CHANNELS];
					static short data[2 * PERIOD];
					p[LEFT] = left->channel()->get_packet();
					p[RIGHT] = right->channel()->get_packet();

					/* convert float to S16LE */
					for (int i = 0; i < 2 * PERIOD; i += 2) {
						data[i] = left->channel()->packet_content(p[LEFT])[i / 2] * 32767;
						data[i + 1] = right->channel()->packet_content(p[RIGHT])[i / 2] * 32767;
					}

					if (verbose)
						PDBG("play packet");

					/* send to driver */
					int err;
					if (audio_out_active)
						if ((err = audio_play(data, 4 * PERIOD)))
							PWRN("Error %d during playback", err);

					/* acknowledge packet to the client */
					if (p[LEFT].valid())
						left->channel()->acknowledge_packet(p[LEFT]);

					if (p[RIGHT].valid())
						right->channel()->acknowledge_packet(p[RIGHT]);
				}
			}

		public:

			Session_component(Channel_number channel, size_t buffer_size,
			                  Rpc_entrypoint &ep, Signal_receiver &sig_rec)
			: Session_rpc_object(_alloc_dataspace(buffer_size), ep), _channel(channel),
			  _process_packet_dispatcher(sig_rec, *this,
			                             &Session_component::_process_packets)
			{
				Audio_out::channel_acquired[_channel] = this;
				Session_rpc_object::_channel.sigh_packet_avail(_process_packet_dispatcher);
				Session_rpc_object::_channel.sigh_ready_to_ack(_process_packet_dispatcher);
			}

			~Session_component()
			{
				Audio_out::channel_acquired[_channel] = 0;

				env()->ram_session()->free(_ds);
			}

			/*********************************
			 ** Audio-out-session interface **
			 *********************************/

			void flush()
			{
				while (channel()->packet_avail())
					channel()->acknowledge_packet(channel()->get_packet());
			}

			void sync_session(Session_capability audio_out_session) { }
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
			if (channel_acquired[channel_number])
				throw Root::Unavailable();
		}

		void release() { }
	};

	typedef Root_component<Session_component, Root_policy> Root_component;

	/*
	 * Root component, handling new session requests.
	 */
	class Root : public Root_component
	{
		private:

			Rpc_entrypoint  &_channel_ep;
			Signal_receiver &_sig_rec;

		protected:

			Session_component *_create_session(const char *args)
			{
				size_t buffer_size =
					Arg_string::find_arg(args, "buffer_size").ulong_value(0);

				if (!audio_out_active)
					throw Root::Unavailable();

				char channel_name[16];
				Channel_number channel_number = INVALID;
				Arg_string::find_arg(args, "channel").string(channel_name,
				                                             sizeof(channel_name),
				                                             "left");
				channel_number_from_string(channel_name, &channel_number);

				return new (md_alloc())
					Session_component(channel_number, buffer_size, _channel_ep, _sig_rec);
			}

		public:

			Root(Rpc_entrypoint &session_ep, Allocator *md_alloc, Signal_receiver &sig_rec)
			: Root_component(&session_ep, md_alloc), _channel_ep(session_ep), _sig_rec(sig_rec)
			{ }
	};
}


int main()
{
	static Signal_receiver recv;
	enum { STACK_SIZE = 1024 * sizeof(addr_t) };
	static Cap_connection cap;
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "audio_ep");

	dde_kit_timer_init(0, 0);

	Irq::init(&recv);
	Service_handler::s()->receiver(&recv);

	/* probe drivers */
	probe_drivers();
	audio_out_active = audio_init() ? false : true;

	if (audio_out_active) {
		static Audio_out::Root audio_root(ep, env()->heap(), recv);
		env()->parent()->announce(ep.manage(&audio_root));
	}

	while (true)
		Service_handler::s()->check_signal(true);

	return 1;
}

