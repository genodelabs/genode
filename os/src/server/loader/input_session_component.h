/*
 * \brief  Input session instance
 * \author Christian Prochaska
 * \date   2010-09-02
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INPUT_SESSION_COMPONENT_H_
#define _INPUT_SESSION_COMPONENT_H_

#include <base/rpc_server.h>
#include <input/event.h>
#include <input_session/client.h>
#include <input_session/input_session.h>

namespace Nitpicker { class Session_component; }

namespace Input {

	class Session_component : public Genode::Rpc_object<Session>
	{
		private:

			Session_client               *_isc;
			Genode::Rpc_entrypoint       *_ep;
			Nitpicker::Session_component *_nsc;
			Input::Event                 *_ev_buf;

		public:

			/**
			 * Constructor
			 */
			Session_component(Genode::Rpc_entrypoint       *ep,
			                  Nitpicker::Session_component *nsc);

			/**
			 * Destructor
			 */
			~Session_component();


			/*****************************
			 ** Input session interface **
			 *****************************/

			Genode::Dataspace_capability dataspace();
			bool is_pending();
			int flush();
	};
}

#endif /* _INPUT_SESSION_COMPONENT_H_ */
