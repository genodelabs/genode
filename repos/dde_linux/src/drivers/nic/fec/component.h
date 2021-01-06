/*
 * \brief  Freescale ethernet driver component
 * \author Stefan Kalkowski
 * \date   2017-11-01
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _SRC__DRIVERS__NIC__FEC__COMPONENT_H_
#define _SRC__DRIVERS__NIC__FEC__COMPONENT_H_

/* Genode includes */
#include <nic/component.h>
#include <root/component.h>

/* local includes */
#include <linux_network_session_base.h>


class Session_component : public Nic::Session_component,
                          public Linux_network_session_base
{
	private:

		bool _has_link = false;

		bool _send();
		void _handle_rx();
		void _handle_packet_stream() override;

	public:

		Session_component(Genode::size_t        const  tx_buf_size,
		                  Genode::size_t        const  rx_buf_size,
		                  Genode::Allocator           &rx_block_md_alloc,
		                  Genode::Env                 &env,
		                  Genode::Session_label const &label);


		/****************************
		 ** Nic::Session_component **
		 ****************************/

		Nic::Mac_address mac_address() override;
		bool link_state() override { return _has_link; }


		/********************************
		 ** Linux_network_session_base **
		 ********************************/

		void link_state(bool link) override;
		void receive(struct sk_buff *skb) override;
};


class Root : public Genode::Root_component<Session_component>
{
	private:

		Genode::Env       &_env;
		Genode::Allocator &_md_alloc;

	protected:

		Session_component *_create_session(const char *args)
		{
			using namespace Genode;

			Session_label const label = label_from_args(args);

			size_t ram_quota   = Arg_string::find_arg(args, "ram_quota"  ).ulong_value(0);
			size_t tx_buf_size = Arg_string::find_arg(args, "tx_buf_size").ulong_value(0);
			size_t rx_buf_size = Arg_string::find_arg(args, "rx_buf_size").ulong_value(0);

			/* deplete ram quota by the memory needed for the session structure */
			size_t session_size = max(4096UL, (unsigned long)sizeof(Session_component));

			/*
			 * Check if donated ram quota suffices for both communication
			 * buffers and check for overflow
			 */
			if ((ram_quota < session_size) ||
			    (tx_buf_size + rx_buf_size < tx_buf_size ||
			     tx_buf_size + rx_buf_size > ram_quota - session_size)) {
				Genode::error("insufficient 'ram_quota', got ", ram_quota, ", "
				              "need ", tx_buf_size + rx_buf_size + session_size);
				throw Genode::Insufficient_ram_quota();
			}

			return new (Root::md_alloc())
			            Session_component(tx_buf_size, rx_buf_size,
			                             _md_alloc, _env, label);
		}

	public:

		Root(Genode::Env &env, Genode::Allocator &md_alloc)
		: Genode::Root_component<Session_component>(&env.ep().rpc_ep(), &md_alloc),
			_env(env), _md_alloc(md_alloc)
		{ }
};

#endif /* _SRC__DRIVERS__NIC__FEC__COMPONENT_H_ */
