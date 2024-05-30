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


class Gui::Session_client : public Genode::Rpc_client<Session>
{
	private:

		Genode::Attached_dataspace _command_ds;

		Command_buffer &_command_buffer;

	public:

		/**
		 * Constructor
		 */
		Session_client(Genode::Region_map &rm, Session_capability session)
		:
			Rpc_client<Session>(session),
			_command_ds(rm, command_dataspace()),
			_command_buffer(*_command_ds.local_addr<Command_buffer>())
		{ }

		Framebuffer::Session_capability framebuffer_session() override {
			return call<Rpc_framebuffer_session>(); }

		Input::Session_capability input_session() override {
			return call<Rpc_input_session>(); }

		View_handle create_view(View_handle parent = View_handle()) override {
			return call<Rpc_create_view>(parent); }

		void destroy_view(View_handle view) override {
			call<Rpc_destroy_view>(view); }

		View_handle view_handle(View_capability view,
		                        View_handle handle = View_handle()) override
		{
			return call<Rpc_view_handle>(view, handle);
		}

		View_capability view_capability(View_handle handle) override {
			return call<Rpc_view_capability>(handle); }

		void release_view_handle(View_handle handle) override {
			call<Rpc_release_view_handle>(handle); }

		Genode::Dataspace_capability command_dataspace() override {
			return call<Rpc_command_dataspace>(); }

		void execute() override
		{
			call<Rpc_execute>();
			_command_buffer.reset();
		}

		Framebuffer::Mode mode() override {
			return call<Rpc_mode>(); }

		void mode_sigh(Genode::Signal_context_capability sigh) override {
			call<Rpc_mode_sigh>(sigh); }

		void buffer(Framebuffer::Mode mode, bool alpha) override {
			call<Rpc_buffer>(mode, alpha); }

		void focus(Gui::Session_capability session) override {
			call<Rpc_focus>(session); }

		/**
		 * Enqueue command to command buffer
		 *
		 * The submitted command is not executed immediately. To execute a
		 * batch of enqueued commands, the 'execute' method must be called.
		 * Only in the corner case when there is not space left in the command
		 * buffer, the 'execute' is called to make room in the buffer.
		 */
		template <typename CMD>
		void enqueue(auto &&... args) { enqueue(Command( CMD { args... } )); }

		void enqueue(Command const &command)
		{
			if (_command_buffer.full())
				execute();

			_command_buffer.enqueue(command);
		}
};

#endif /* _INCLUDE__GUI_SESSION__CLIENT_H_ */
