/*
 * \brief  Nic session component and root
 * \author Martin Stein
 * \date   2022-02-12
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _NIC_H_
#define _NIC_H_

/* base includes */
#include <util/arg_string.h>
#include <root/component.h>

/* os includes */
#include <nic/component.h>

namespace Black_hole {

	using namespace Genode;

	class Nic_session;
	class Nic_root;
}


class Black_hole::Nic_session : public Nic::Session_component
{
	public:

		Nic_session(size_t const tx_buf_size,
		            size_t const rx_buf_size,
		            Allocator   &rx_block_md_alloc,
		            Env         &env)
		:
			Nic::Session_component(tx_buf_size, rx_buf_size, CACHED,
			                       rx_block_md_alloc, env)
		{ }

		Nic::Mac_address mac_address() override
		{
			char buf[] = { 2, 3, 4, 5, 6, 7 };
			Nic::Mac_address result((void*)buf);
			return result;
		}

		bool link_state() override { return true; }

		void _handle_packet_stream() override
		{
			while (_tx.sink()->packet_avail()) {

				if (!_tx.sink()->ready_to_ack()) {
					return;
				}
				Packet_descriptor const pkt { _tx.sink()->get_packet() };
				if (!pkt.size() || !_tx.sink()->packet_valid(pkt)) {
					continue;
				}
				_tx.sink()->acknowledge_packet(pkt);
			}
		}
};


class Black_hole::Nic_root : public Root_component<Nic_session>
{
	private:

		Env  &_env;

	protected:

		Nic_session *_create_session(char const *args) override
		{
			size_t ram_quota   = Arg_string::find_arg(args, "ram_quota"  ).ulong_value(0);
			size_t tx_buf_size = Arg_string::find_arg(args, "tx_buf_size").ulong_value(0);
			size_t rx_buf_size = Arg_string::find_arg(args, "rx_buf_size").ulong_value(0);

			/* deplete ram quota by the memory needed for the session structure */
			size_t session_size = max(4096UL, (size_t)sizeof(Nic_session));
			if (ram_quota < session_size)
				throw Insufficient_ram_quota();

			/*
			 * Check if donated ram quota suffices for both communication
			 * buffers and check for overflow
			 */
			if (tx_buf_size + rx_buf_size < tx_buf_size ||
			    tx_buf_size + rx_buf_size > ram_quota - session_size) {
				error("insufficient 'ram_quota', got ", ram_quota, ", "
				      "need ", tx_buf_size + rx_buf_size + session_size);
				throw Insufficient_ram_quota();
			}

			return new (md_alloc()) Nic_session(tx_buf_size, rx_buf_size,
			                                    *md_alloc(), _env);
		}

	public:

		Nic_root(Env       &env,
		         Allocator &md_alloc)
		:
			Root_component<Nic_session>(&env.ep().rpc_ep(), &md_alloc),
			_env(env)
		{ }
};

#endif /* _NIC_H_ */
