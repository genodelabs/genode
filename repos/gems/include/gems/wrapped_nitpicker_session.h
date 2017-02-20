/*
 * \brief  Wrapper of a nitpicker session
 * \author Norman Feske
 * \date   2014-10-01
 *
 * This utility is intended to reduce repetitive boiler-plate code of
 * components that intercept the nitpicker session interface. By default,
 * all RPC function calls are forwarded to the wrapped session. So the
 * implementations have to override only those functions that need
 * customizations.
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__GEMS__WRAPPED_NITPICKER_SESSION_H_
#define _INCLUDE__GEMS__WRAPPED_NITPICKER_SESSION_H_

/* Genode includes */
#include <base/rpc_server.h>
#include <nitpicker_session/nitpicker_session.h>

class Wrapped_nitpicker_session : public Genode::Rpc_object<Nitpicker::Session>
{
	private:

		Nitpicker::Session &_session;

	public:

		typedef Nitpicker::View_capability      View_capability;
		typedef Nitpicker::Session::View_handle View_handle;

		/**
		 * Constructor
		 *
		 * \param session  interface of wrapped nitpicker session
		 */
		Wrapped_nitpicker_session(Nitpicker::Session &session) : _session(session) { }

		Framebuffer::Session_capability framebuffer_session() override
		{
			return _session.framebuffer_session();
		}

		Input::Session_capability input_session() override
		{
			return _session.input_session();
		}

		View_handle create_view(View_handle parent) override
		{
			return _session.create_view(parent);
		}

		void destroy_view(View_handle view) override
		{
			_session.destroy_view(view);
		}

		View_handle view_handle(View_capability view_cap, View_handle handle) override
		{
			return _session.view_handle(view_cap, handle);
		}

		View_capability view_capability(View_handle view) override
		{
			return _session.view_capability(view);
		}

		void release_view_handle(View_handle view) override
		{
			_session.release_view_handle(view);
		}

		Genode::Dataspace_capability command_dataspace() override
		{
			return _session.command_dataspace();
		}

		void execute() override
		{
			_session.execute();
		}

		Framebuffer::Mode mode() override
		{
			return _session.mode();
		}

		void mode_sigh(Genode::Signal_context_capability sigh) override
		{
			_session.mode_sigh(sigh);
		}

		void buffer(Framebuffer::Mode mode, bool use_alpha) override
		{
			_session.buffer(mode, use_alpha);
		}

		void focus(Genode::Capability<Nitpicker::Session> session)
		{
			_session.focus(session);
		}
};

#endif /* _INCLUDE__GEMS__WRAPPED_NITPICKER_SESSION_H_ */
