/*
 * \brief   Framebuffer root
 * \author  Christian Prochaska
 * \date    2012-04-02
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */


#ifndef _FRAMEBUFFER_ROOT_H_
#define _FRAMEBUFFER_ROOT_H_

/* Genode includes */
#include <root/component.h>

#include "framebuffer_session_component.h"

namespace Framebuffer {

	/**
	 * Shortcut for single-client root component
	 */
	typedef Genode::Root_component<Session_component, Genode::Single_client> Root_component;


	class Root : public Root_component
	{
		private:

			QNitpickerViewWidget &_nitpicker_view_widget;
			int                   _max_width;
			int                   _max_height;

		protected:

			Session_component *_create_session(const char *args)
			{
				return new (md_alloc())
				  Session_component(args, _nitpicker_view_widget,
				                          _max_width, _max_height);
			}

		public:

			Root(Genode::Rpc_entrypoint *session_ep,
				 Genode::Allocator      *md_alloc,
				 QNitpickerViewWidget   &nitpicker_view_widget,
				 int max_width = 0,
				 int max_height = 0)
			: Root_component(session_ep, md_alloc),
			  _nitpicker_view_widget(nitpicker_view_widget),
			  _max_width(max_width),
			  _max_height(max_height) { }

	};

}

#endif /* _FRAMEBUFFER_ROOT_H_ */
