/*
 * \brief  Client-side GUI session interface
 * \author Norman Feske
 * \date   2006-08-23
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__GUI_SESSION__CLIENT_H_
#define _INCLUDE__GUI_SESSION__CLIENT_H_

#include <gui_session/capability.h>
#include <base/rpc_client.h>

namespace Gui { struct Session_client; }


struct Gui::Session_client : Rpc_client<Session>
{
	Session_client(Capability<Session> session) : Rpc_client<Session>(session) { }

	Framebuffer::Session_capability framebuffer() override {
		return call<Rpc_framebuffer>(); }

	Input::Session_capability input() override {
		return call<Rpc_input>(); }

	View_result view(View_id id, View_attr const &attr) override {
		return call<Rpc_view>(id, attr); }

	Child_view_result child_view(View_id id, View_id parent, View_attr const &attr) override {
		return call<Rpc_child_view>(id, parent, attr); }

	void destroy_view(View_id view) override {
		call<Rpc_destroy_view>(view); }

	Associate_result associate(View_id id, View_capability view) override {
		return call<Rpc_associate>(id, view); }

	View_capability_result view_capability(View_id id) override {
		return call<Rpc_view_capability>(id); }

	void release_view_id(View_id id) override {
		call<Rpc_release_view_id>(id); }

	Dataspace_capability command_dataspace() override {
		return call<Rpc_command_dataspace>(); }

	void execute() override { call<Rpc_execute>(); }

	Framebuffer::Mode mode() override {
		return call<Rpc_mode>(); }

	void mode_sigh(Signal_context_capability sigh) override {
		call<Rpc_mode_sigh>(sigh); }

	Buffer_result buffer(Framebuffer::Mode mode, bool alpha) override {
		return call<Rpc_buffer>(mode, alpha); }

	void focus(Gui::Session_capability session) override {
		call<Rpc_focus>(session); }
};

#endif /* _INCLUDE__GUI_SESSION__CLIENT_H_ */
