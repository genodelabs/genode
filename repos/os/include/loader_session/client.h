/*
 * \brief  Client-side loader-session interface
 * \author Christian Prochaska
 * \date   2009-10-05
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__LOADER_SESSION__CLIENT_H_
#define _INCLUDE__LOADER_SESSION__CLIENT_H_

#include <loader_session/loader_session.h>
#include <loader_session/capability.h>
#include <base/rpc_client.h>
#include <os/alarm.h>

namespace Loader { struct Session_client; }


struct Loader::Session_client : Genode::Rpc_client<Session>
{
	explicit Session_client(Loader::Session_capability session)
	: Rpc_client<Session>(session) { }

	Dataspace_capability alloc_rom_module(Name const &name,
	                                      size_t size) override {
		return call<Rpc_alloc_rom_module>(name, size); }

	void commit_rom_module(Name const &name) override {
		call<Rpc_commit_rom_module>(name); }

	void cap_quota(Cap_quota limit) override {
		call<Rpc_cap_quota>(limit); }

	void ram_quota(Ram_quota quantum) override {
		call<Rpc_ram_quota>(quantum); }

	void constrain_geometry(Area size) override {
		call<Rpc_constrain_geometry>(size); }

	void parent_view(Nitpicker::View_capability view) override {
		call<Rpc_parent_view>(view); }

	void view_ready_sigh(Signal_context_capability sigh) override {
		call<Rpc_view_ready_sigh>(sigh); }

	void fault_sigh(Signal_context_capability sigh) override {
		call<Rpc_fault_sigh>(sigh); }

	void start(Name const &binary, Name const &label = "") override {
		call<Rpc_start>(binary, label); }

	void view_geometry(Rect rect, Point offset) override {
		call<Rpc_view_geometry>(rect, offset); }

	Area view_size() const override {
		return call<Rpc_view_size>(); }
};

#endif /* _INCLUDE__PLUGIN_SESSION__CLIENT_H_ */
