/*
 * \brief  Client-side loader-session interface
 * \author Christian Prochaska
 * \date   2009-10-05
 */

/*
 * Copyright (C) 2009-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__LOADER_SESSION__CLIENT_H_
#define _INCLUDE__LOADER_SESSION__CLIENT_H_

#include <loader_session/loader_session.h>
#include <loader_session/capability.h>
#include <base/rpc_client.h>
#include <os/alarm.h>

namespace Loader {

	struct Session_client : Rpc_client<Session>
	{
		explicit Session_client(Loader::Session_capability session)
		: Rpc_client<Session>(session) { }

		Dataspace_capability alloc_rom_module(Name const &name, size_t size) {
			return call<Rpc_alloc_rom_module>(name, size); }

		void commit_rom_module(Name const &name) {
			call<Rpc_commit_rom_module>(name); }

		void ram_quota(size_t quantum) {
			call<Rpc_ram_quota>(quantum); }

		void constrain_geometry(int width, int height) {
			call<Rpc_constrain_geometry>(width, height); }

		void view_ready_sigh(Signal_context_capability sigh) {
			call<Rpc_view_ready_sigh>(sigh); }

		void start(Name const &binary, Name const &label = "") {
			call<Rpc_start>(binary, label); }

		Nitpicker::View_capability view() {
			return call<Rpc_view>(); }

		View_geometry view_geometry() {
			return call<Rpc_view_geometry>(); }
	};
}

#endif /* _INCLUDE__PLUGIN_SESSION__CLIENT_H_ */
