/*
 * \brief  Nic root and session component
 * \author Johannes Schlatow
 * \date   2022-06-16
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _NIC_ROOT_H_
#define _NIC_ROOT_H_

/* local includes */
#include <interface.h>

/* Genode includes */
#include <root/component.h>
#include <nic/component.h>
#include <base/attached_rom_dataspace.h>
#include <os/session_policy.h>

namespace Nic_perf {
	class Nic_session_component;
	class Nic_root;

	using namespace Genode;
}


class Nic_perf::Nic_session_component : public Nic::Session_component
{
	private:

		Interface _interface;

		static Mac_address _default_mac_address()
		{
			char buf[] = {2,3,4,5,6,7};
			Mac_address result((void*)buf);
			return result;
		}

		void _handle_packet_stream() override {
			_interface.handle_packet_stream(); }

	public:

		Nic_session_component(size_t        const  tx_buf_size,
		                      size_t        const  rx_buf_size,
		                      Allocator           &rx_block_md_alloc,
		                      Env                 &env,
		                      Session_label const &label,
		                      Xml_node      const &policy,
		                      Interface_registry  &registry,
		                      Timer::Connection   &timer)
		:
			Nic::Session_component(tx_buf_size, rx_buf_size, CACHED,
			                       rx_block_md_alloc, env),
			_interface(registry, label, policy, true, _default_mac_address(),
			           *_rx.source(), *_tx.sink(), timer)
		{ _interface.handle_packet_stream(); }

		/*****************************
		 * Session_component methods *
		 *****************************/

		Nic::Mac_address mac_address() override {
			char buf[] = {2,3,4,5,6,8};
			Mac_address result((void*)buf);
			return result;
		}

		bool link_state() override
		{
			/* XXX always return true, for now */
			return true;
		}
};


class Nic_perf::Nic_root : public Root_component<Nic_session_component>
{
	private:
		Env                    &_env;
		Attached_rom_dataspace &_config;
		Interface_registry     &_registry;
		Timer::Connection      &_timer;

	protected:

		Nic_session_component *_create_session(char const *args) override
		{
			size_t ram_quota   = Arg_string::find_arg(args, "ram_quota"  ).ulong_value(0);
			size_t tx_buf_size = Arg_string::find_arg(args, "tx_buf_size").ulong_value(0);
			size_t rx_buf_size = Arg_string::find_arg(args, "rx_buf_size").ulong_value(0);

			/* deplete ram quota by the memory needed for the session structure */
			size_t session_size = max(4096UL, (size_t)sizeof(Nic_session_component));
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

			Session_label label = label_from_args(args);

			Session_policy policy(label, _config.xml());

			return new (md_alloc()) Nic_session_component(tx_buf_size, rx_buf_size,
			                                              *md_alloc(), _env, label, policy,
			                                              _registry, _timer);
		}

	public:

		Nic_root(Env                    &env,
		         Allocator              &md_alloc,
		         Interface_registry     &registry,
		         Attached_rom_dataspace &config,
		         Timer::Connection      &timer)
		:
			Root_component<Nic_session_component>(&env.ep().rpc_ep(), &md_alloc),
			_env(env),
			_config(config),
			_registry(registry),
			_timer(timer)
		{ }
};

#endif /* _NIC_ROOT_H_ */
