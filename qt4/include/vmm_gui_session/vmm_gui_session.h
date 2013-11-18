 /*
 * \brief  VM-session interface
 * \author Stefan Kalkowski
 * \date   2012-10-02
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__VMM_GUI_SESSION__VMM_GUI_SESSION_H_
#define _INCLUDE__VMM_GUI_SESSION__VMM_GUI_SESSION_H_

/* Genode includes */
#include <cpu/cpu_state.h>
#include <base/rpc_args.h>
#include <base/signal.h>
#include <session/session.h>
#include <nitpicker_view/capability.h>

namespace Vmm_gui {

	struct Session : Genode::Session
	{
		static const char *service_name() { return "Vmmgui"; }

		typedef Genode::Rpc_in_buffer<sizeof(Genode::Cpu_state_modes)> Vm_state;

		/**
		 * Destructor
		 */
		virtual ~Session() { }

		virtual void show_view(Nitpicker::View_capability cap,
		                       int w, int h) = 0;

		virtual void play_resume_sigh(Genode::Signal_context_capability) = 0;
		virtual void stop_sigh(Genode::Signal_context_capability)        = 0;
		virtual void bomb_sigh(Genode::Signal_context_capability)        = 0;
		virtual void power_sigh(Genode::Signal_context_capability)       = 0;
		virtual void fullscreen_sigh(Genode::Signal_context_capability)  = 0;

		virtual void set_state(Vm_state const &vm_state) {}


		/*********************
		 ** RPC declaration **
		 *********************/

		GENODE_RPC(Rpc_show_view, void, show_view,
		           Nitpicker::View_capability, int, int);
		GENODE_RPC(Rpc_play, void, play_resume_sigh,
		           Genode::Signal_context_capability);
		GENODE_RPC(Rpc_stop, void, stop_sigh,
		           Genode::Signal_context_capability);
		GENODE_RPC(Rpc_bomb, void, bomb_sigh,
		           Genode::Signal_context_capability);
		GENODE_RPC(Rpc_power, void, power_sigh,
		           Genode::Signal_context_capability);
		GENODE_RPC(Rpc_fs, void, fullscreen_sigh,
		           Genode::Signal_context_capability);
		GENODE_RPC(Rpc_set_state, void, set_state, Vm_state const &);

		GENODE_RPC_INTERFACE(Rpc_show_view, Rpc_play, Rpc_stop, Rpc_bomb,
		                     Rpc_power, Rpc_fs, Rpc_set_state);
	};
}

#endif /* _INCLUDE__VMM_GUI_SESSION__VMM_GUI_SESSION_H_ */
