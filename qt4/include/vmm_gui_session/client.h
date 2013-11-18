/*
 * \brief  Client-side VMM GUI session interface
 * \author Stefan Kalkowski
 * \date   2013-04-17
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__VMM_GUI_SESSION__CLIENT_H_
#define _INCLUDE__VMM_GUI_SESSION__CLIENT_H_

/* Genode includes */
#include <vmm_gui_session/capability.h>
#include <base/rpc_client.h>

namespace Vmm_gui
{
	/**
	 * Client-side VMM GUI session interface
	 */
	struct Session_client : Genode::Rpc_client<Session>
	{
		/**
		 * Constructor
		 */
		explicit Session_client(Capability cap)
		: Genode::Rpc_client<Session>(cap) { }


		/*******************************
		 ** VMM GUI session interface **
		 *******************************/

		void show_view(Nitpicker::View_capability cap, int w, int h) {
			call<Rpc_show_view>(cap, w, h); }

		void play_resume_sigh(Genode::Signal_context_capability handler) {
			call<Rpc_play>(handler); }

		void stop_sigh(Genode::Signal_context_capability handler) {
			call<Rpc_stop>(handler); }

		void bomb_sigh(Genode::Signal_context_capability handler) {
			call<Rpc_bomb>(handler); }

		void power_sigh(Genode::Signal_context_capability handler) {
			call<Rpc_power>(handler); }

		void fullscreen_sigh(Genode::Signal_context_capability handler) {
			call<Rpc_fs>(handler); }

		void set_state(Genode::Cpu_state_modes const *cpu_state)
		{
			Vm_state state((const char *)cpu_state, sizeof(Genode::Cpu_state_modes));
			call<Rpc_set_state>(state);
		}
	};
}

#endif /* _INCLUDE__VMM_GUI_SESSION__CLIENT_H_ */
