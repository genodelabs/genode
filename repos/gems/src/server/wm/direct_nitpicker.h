/*
 * \brief  Pass-through nitpicker service announced to the outside world
 * \author Norman Feske
 * \date   2015-09-29
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _DIRECT_NITPICKER_H_
#define _DIRECT_NITPICKER_H_

/* Genode includes */
#include <os/session_policy.h>
#include <nitpicker_session/connection.h>

namespace Wm { class Direct_nitpicker_session; }


class Wm::Direct_nitpicker_session : public Genode::Rpc_object<Nitpicker::Session>
{
	private:

		Genode::Session_label _session_label;
		Nitpicker::Connection _session { _session_label.string() };

	public:

		/**
		 * Constructor
		 */
		Direct_nitpicker_session(Genode::Session_label const &session_label)
		:
			_session_label(session_label)
		{ }

		void upgrade(char const *args)
		{
			Genode::env()->parent()->upgrade(_session, args);
		}


		/*********************************
		 ** Nitpicker session interface **
		 *********************************/
		
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

		View_handle view_handle(Nitpicker::View_capability view_cap, View_handle handle) override
		{
			return _session.view_handle(view_cap, handle);
		}

		Nitpicker::View_capability view_capability(View_handle view) override
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

#endif /* _DIRECT_NITPICKER_H_ */
