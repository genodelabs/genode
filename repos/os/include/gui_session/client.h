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
#include <base/attached_dataspace.h>

namespace Gui { struct Session_client; }


struct Gui::Session_client : Rpc_client<Session>
{
		/**
		 * Constructor
		 */
		Session_client(Session_capability session) : Rpc_client<Session>(session) { }

		Framebuffer::Session_capability framebuffer_session() override {
			return call<Rpc_framebuffer_session>(); }

		Input::Session_capability input_session() override {
			return call<Rpc_input_session>(); }

		Create_view_result create_view() override {
			return call<Rpc_create_view>(); }

		Create_child_view_result create_child_view(View_handle parent) override {
			return call<Rpc_create_child_view>(parent); }

		void destroy_view(View_handle view) override {
			call<Rpc_destroy_view>(view); }

		View_handle_result view_handle(View_capability view,
		                               View_handle handle = View_handle()) override
		{
			return call<Rpc_view_handle>(view, handle);
		}

		View_capability view_capability(View_handle handle) override {
			return call<Rpc_view_capability>(handle); }

		void release_view_handle(View_handle handle) override {
			call<Rpc_release_view_handle>(handle); }

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
