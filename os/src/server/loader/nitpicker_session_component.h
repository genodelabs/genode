/*
 * \brief  Instance of the Nitpicker session interface
 * \author Christian Prochaska
 * \date   2009-10-05
 */

/*
 * Copyright (C) 2009-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _NITPICKER_SESSION_COMPONENT_H_
#define _NITPICKER_SESSION_COMPONENT_H_

/* Genode includes */
#include <nitpicker_session/connection.h>
#include <nitpicker_session/nitpicker_session.h>
#include <nitpicker_view/capability.h>
#include <os/timed_semaphore.h>

/* local includes */
#include <input_root.h>
#include <loader_view_component.h>
#include <nitpicker_view_component.h>

namespace Nitpicker {

	class Session_component : public Genode::Rpc_object<Session>
	{
		private:

			Nitpicker::Connection _nitpicker;
			View_capability       _nitpicker_view;
			View_capability       _proxy_view_cap;
			View_capability       _loader_view_cap;

			Genode::Rpc_entrypoint    *_ep;
			Genode::Timed_semaphore   *_ready_sem;
			View_component            *_proxy_view;
			Loader_view_component     *_loader_view;

			Input::Root                _input_root;
			Input::Session_capability  _proxy_input_session;

		public:

			/**
			 * Constructor
			 *
			 * \param args         session-construction arguments, in
			 *                     particular the filename
			 */
			Session_component(Genode::Rpc_entrypoint  *ep,
			                  Genode::Timed_semaphore *ready_sem,
			                  const char              *args);

			/**
			 * Destructor
			 */
			~Session_component();


			/*********************************
			 ** Nitpicker session interface **
			 *********************************/

			Framebuffer::Session_capability framebuffer_session();
			Input::Session_capability input_session();
			View_capability create_view();
			void destroy_view(View_capability view);
			int background(View_capability view);


			/**
			 * Return the client-specific wrapper view for the Nitpicker view
			 * showing the child content
			 */
			View_capability loader_view(int *w, int *h, int *buf_x, int *buf_y);

			/**
			 * Request real input sub-session
			 *
			 * This method is not accessible to ipc clients.
			 */
			virtual Input::Session_capability real_input_session();

			/**
			 * Request child view component
			 *
			 * This method is meant to be used by the input wrapper and not
			 * accessible to ipc clients.
			 */
			View_component *proxy_view_component();

			/**
			 * Request client view component
			 *
			 * This method is meant to be used by the input wrapper and not
			 * accessible to ipc clients.
			 */
			Loader_view_component *loader_view_component();
	};
}

#endif /* _NITPICKER_SESSION_COMPONENT_H_ */
