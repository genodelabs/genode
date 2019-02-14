/**
 * \brief  Simple single client NIC Root
 * \author Sebastian Sumpf
 * \date   2015-06-23
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__NIC__ROOT_H_
#define _INCLUDE__NIC__ROOT_H_

#include <nic/component.h>
#include <root/component.h>

namespace Nic {
	using namespace Genode;

	template <class SESSION_COMPONENT> class Root;
};


template <class SESSION_COMPONENT>
class Nic::Root : public Genode::Root_component<SESSION_COMPONENT,
                                                Genode::Single_client>
{
	private:

		Env       &_env;
		Allocator &_md_alloc;

	protected:

		SESSION_COMPONENT *_create_session(const char *args) override
		{
			using namespace Genode;

			size_t ram_quota   = Arg_string::find_arg(args, "ram_quota"  ).ulong_value(0);
			size_t tx_buf_size = Arg_string::find_arg(args, "tx_buf_size").ulong_value(0);
			size_t rx_buf_size = Arg_string::find_arg(args, "rx_buf_size").ulong_value(0);

			/* deplete ram quota by the memory needed for the session structure */
			size_t session_size = max(4096UL, (unsigned long)sizeof(SESSION_COMPONENT));
			if (ram_quota < session_size)
				throw Genode::Insufficient_ram_quota();

			/*
			 * Check if donated ram quota suffices for both communication
			 * buffers and check for overflow
			 */
			if (tx_buf_size + rx_buf_size < tx_buf_size ||
			    tx_buf_size + rx_buf_size > ram_quota - session_size) {
				Genode::error("insufficient 'ram_quota', got ", ram_quota, ", "
				              "need ", tx_buf_size + rx_buf_size + session_size);
				throw Genode::Insufficient_ram_quota();
			}

			return new (Root::md_alloc())
			            SESSION_COMPONENT(tx_buf_size, rx_buf_size,
			                             _md_alloc, _env);
		}

	public:

		Root(Genode::Env &env, Genode::Allocator &md_alloc)
		: Genode::Root_component<SESSION_COMPONENT, Genode::Single_client>(&env.ep().rpc_ep(), &md_alloc),
			_env(env), _md_alloc(md_alloc)
		{ }
};

#endif /* _INCLUDE__NIC__ROOT_H_ */
