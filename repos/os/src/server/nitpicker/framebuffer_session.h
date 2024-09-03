/*
 * \brief  Framebuffer sub session as part of the GUI session
 * \author Norman Feske
 * \date   2017-11-16
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _FRAMEBUFFER_SESSION_COMPONENT_H_
#define _FRAMEBUFFER_SESSION_COMPONENT_H_

/* local includes */
#include <buffer.h>

namespace Framebuffer {

	using namespace Nitpicker;
	using Nitpicker::Rect;
	using Nitpicker::Point;
	using Nitpicker::Area;

	class Session_component;
}


class Framebuffer::Session_component : public Rpc_object<Session>
{
	private:

		/*
		 * Noncopyable
		 */
		Session_component(Session_component const &);
		Session_component &operator = (Session_component const &);

		Entrypoint               &_ep;
		View_stack               &_view_stack;
		Nitpicker::Gui_session   &_session;
		Buffer_provider          &_buffer_provider;
		Signal_context_capability _mode_sigh { };
		Signal_context_capability _sync_sigh { };
		Framebuffer::Mode         _mode { };
		bool                      _alpha = false;

	public:

		/**
		 * Constructor
		 */
		Session_component(Entrypoint             &ep,
		                  View_stack             &view_stack,
		                  Nitpicker::Gui_session &session,
		                  Buffer_provider        &buffer_provider)
		:
			_ep(ep),
			_view_stack(view_stack),
			_session(session),
			_buffer_provider(buffer_provider)
		{
			_ep.manage(*this);
		}

		~Session_component() { _ep.dissolve(*this); }

		/**
		 * Change virtual framebuffer mode
		 *
		 * Called by Nitpicker::Gui_session when re-dimensioning the buffer.
		 *
		 * The new mode does not immediately become active. The client can
		 * keep using an already obtained framebuffer dataspace. However,
		 * we inform the client about the mode change via a signal. If the
		 * client calls 'dataspace' the next time, the new mode becomes
		 * effective.
		 */
		void notify_mode_change(Framebuffer::Mode mode, bool alpha)
		{
			_mode  = mode;
			_alpha = alpha;

			if (_mode_sigh.valid())
				Signal_transmitter(_mode_sigh).submit();
		}

		void submit_sync()
		{
			if (_sync_sigh.valid())
				Signal_transmitter(_sync_sigh).submit();
		}


		/************************************
		 ** Framebuffer::Session interface **
		 ************************************/

		Dataspace_capability dataspace() override
		{
			return _buffer_provider.realloc_buffer(_mode, _alpha);
		}

		Mode mode() const override { return _mode; }

		void mode_sigh(Signal_context_capability sigh) override
		{
			_mode_sigh = sigh;
		}

		void sync_sigh(Signal_context_capability sigh) override
		{
			_sync_sigh = sigh;
		}

		void refresh(int x, int y, int w, int h) override;
};

#endif /* _FRAMEBUFFER_SESSION_COMPONENT_H_ */
