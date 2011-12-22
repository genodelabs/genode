/*
 * \brief  Nitpicker root interface
 * \author Christian Prochaska
 * \date   2010-07-12
 */

/*
 * Copyright (C) 2010-2011 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _NITPICKER_ROOT_H_
#define _NITPICKER_ROOT_H_

/* Genode includes */
#include <root/component.h>

/* local includes */
#include <nitpicker_session_component.h>

namespace Nitpicker {

	/**
	 * Shortcut for single-client root component
	 */
	typedef Genode::Root_component<Session_component, Genode::Single_client> Root_component;

	class Root : public Root_component
	{
		private:

			Genode::Timed_semaphore *_ready_sem;
			Nitpicker::Session_component *_nsc;

		protected:

			Nitpicker::Session_component *_create_session(const char *args)
			{
				_nsc = new (md_alloc()) Nitpicker::Session_component(ep(), _ready_sem, args);
				return _nsc;
			}

		public:

			/**
			 * Constructor
			 *
			 * \param session_ep  entry point for managing ram session objects
			 * \param md_alloc    meta-data allocator to be used by root component
			 */
			Root(Genode::Rpc_entrypoint  *session_ep,
			     Genode::Allocator       *md_alloc,
			     Genode::Timed_semaphore *ready_sem)
			: Root_component(session_ep, md_alloc), _ready_sem(ready_sem), _nsc(0)
			{ }

			~Root()
			{
				Genode::destroy(md_alloc(), _nsc);
			}

			void close(Genode::Session_capability cap)
			{
				destroy(md_alloc(), _nsc);
				_nsc = 0;
			}

			Nitpicker::View_capability view(int *w, int *h, int *buf_x, int *buf_y)
			{
				if (_nsc) {
					return _nsc->loader_view(w, h, buf_x, buf_y);
				} else {
					PERR("the plugin has not created a Nitpicker session yet");
					return Nitpicker::View_capability();
				}
			}
	};
}

#endif /* _NITPICKER_ROOT_H_ */
