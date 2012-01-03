/*
 * \brief  Input root interface providing monitored input session to the child
 * \author Christian Prochaska
 * \date   2010-09-02
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INPUT_ROOT_H_
#define _INPUT_ROOT_H_

#include <root/component.h>
#include <nitpicker_session_component.h>
#include <input_session_component.h>

namespace Input {

	class Root : public Genode::Root_component<Session_component>
	{
		private:

			/* local Input session component */
			Session_component *_isc;

			Nitpicker::Session_component *_nsc;

		protected:

			Session_component *_create_session(const char *args)
			{
				if (!_isc) {
					_isc = new (md_alloc()) Session_component(ep(), _nsc);
					return _isc;
				} else {
					PERR("Only one input session is allowed in the current implementation.");
					return 0;
				}
			}

		public:

			/**
			 * Constructor
			 *
			 * \param session_ep  entry point for managing ram session objects
			 * \param md_alloc    meta-data allocator to be used by root component
			 */
			Root(Genode::Rpc_entrypoint       *session_ep,
			     Genode::Allocator            *md_alloc,
			     Nitpicker::Session_component *nsc)
			: Genode::Root_component<Session_component>(session_ep, md_alloc),
			  _isc(0), _nsc(nsc)
			{ }

			~Root() { destroy(md_alloc(), _isc); }

			void close(Genode::Session_capability cap)
			{
				destroy(md_alloc(), _isc);
				_isc = 0;
			}
	};
}

#endif /* _INPUT_ROOT_H_ */
