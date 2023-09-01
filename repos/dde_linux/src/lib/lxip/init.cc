/*
 * \brief  lx_kit C++ initialization and client handling
 * \author Sebastian Sumpf
 * \date   2024-01-29
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_kit/env.h>
#include <lx_emul/init.h>
#include <lx_emul/task.h>

#include <genode_c_api/nic_client.h>
#include <genode_c_api/socket.h>

#include "lx_user.h"
#include "net_driver.h"


using namespace Genode;

struct Main
{
	Env &env;
	genode_socket_io_progress *io_progress;

	Signal_handler<Main> schedule_handler   { env.ep(), *this,
		&Main::handle_schedule };

	Io_signal_handler<Main> nic_client_handler { env.ep(), *this,
		&Main::handle_nic_client };

	Main(Env &env, genode_socket_io_progress *io_progress)
	: env(env), io_progress(io_progress)
	{ }

	void handle_schedule()
	{
		Lx_kit::env().scheduler.execute();

		if (io_progress && io_progress->callback)
			io_progress->callback(io_progress->data);
	}

	void handle_nic_client()
	{

		lx_emul_task_unblock(lx_nic_client_rx_task());
		Lx_kit::env().scheduler.execute();

		if (io_progress && io_progress->callback)
			io_progress->callback(io_progress->data);
	}

	void init()
	{
		genode_nic_client_init(genode_env_ptr(env),
		                       genode_allocator_ptr(Lx_kit::env().heap),
		                       genode_signal_handler_ptr(nic_client_handler));
	}

	Main(const Main&) = delete;
	Main operator=(const Main&) = delete;
};


void genode_socket_init(struct genode_env *_env,
                        struct genode_socket_io_progress *io_progress)
{
	Env &env = *static_cast<Env *>(_env);
	static Main main { env, io_progress };

	Lx_kit::initialize(env, main.schedule_handler);

	main.init();

	/* must be called before initcalls */
	lx_user_configure_ip_stack();

	lx_emul_start_kernel(nullptr);

	/* wait to finish initialization before returning to callee */
	lx_emul_execute_kernel_until(lx_user_startup_complete, nullptr);
}
