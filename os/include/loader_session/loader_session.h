/*
 * \brief  Loader session interface
 * \author Christian Prochaska
 * \date   2009-10-05
 */

/*
 * Copyright (C) 2009-2011 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__LOADER_SESSION__LOADER_SESSION_H_
#define _INCLUDE__LOADER_SESSION__LOADER_SESSION_H_

#include <base/rpc.h>
#include <base/rpc_args.h>
#include <dataspace/capability.h>
#include <nitpicker_view/capability.h>
#include <rom_session/connection.h>
#include <os/alarm.h>
#include <session/session.h>

namespace Loader {

	struct Session : Genode::Session
	{
		/**
		 * Exception type
		 */
		class Start_failed { };
		class Rom_access_failed      : public Start_failed { };
		class Plugin_start_timed_out : public Start_failed { };
		class Invalid_args           : public Start_failed { };

		typedef Genode::Rpc_in_buffer<64>  Name;
		typedef Genode::Rpc_in_buffer<160> Start_args;

		static const char *service_name() { return "Loader"; }

		virtual ~Session() { }

		virtual Genode::Dataspace_capability dataspace() = 0;

		/**
		 * Start program or subsystem contained in dataspace
		 *
		 * \throw Rom_access_failed
		 * \throw Plugin_start_timed_out
		 */
		virtual void start(Start_args const &args,
		                   int max_width, int max_height,
		                   Genode::Alarm::Time timeout,
		                   Name const &name) = 0;

		virtual Nitpicker::View_capability view(int *w, int *h, int *buf_x, int *buf_y) = 0;


		/*******************
		 ** RPC interface **
		 *******************/

		GENODE_RPC(Rpc_dataspace, Genode::Dataspace_capability, dataspace);
		GENODE_RPC_THROW(Rpc_start, void, start,
		                 GENODE_TYPE_LIST(Start_failed, Rom_access_failed, Plugin_start_timed_out, Invalid_args),
		                 Start_args const &, int, int, Genode::Alarm::Time, Name const &);
		GENODE_RPC(Rpc_view, Nitpicker::View_capability, view, int *, int *, int *, int *);

		GENODE_RPC_INTERFACE(Rpc_dataspace, Rpc_start, Rpc_view);
	};
}

#endif /* _INCLUDE__LOADER_SESSION__LOADER_SESSION_H_ */
