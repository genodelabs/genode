/*
 * \brief   Framebuffer session component
 * \author  Christian Prochaska
 * \date    2012-04-02
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */


#ifndef _FRAMEBUFFER_SESSION_COMPONENT_H_
#define _FRAMEBUFFER_SESSION_COMPONENT_H_

/* Genode includes */
#include <base/rpc_server.h>
#include <framebuffer_session/client.h>
#include <nitpicker_session/connection.h>

/* Qt4 includes */
#include <qnitpickerviewwidget/qnitpickerviewwidget.h>


namespace Framebuffer {

	class Session_component : public Genode::Rpc_object<Session>
	{
		private:

			Nitpicker::Connection  _nitpicker;
			Session_client         _framebuffer;

			int _limited_size(int requested_size, int max_size);

		public:

			/**
			 * Constructor
			 */
			Session_component(const char *args,
			                  QNitpickerViewWidget &nitpicker_view_widget,
			                  int max_width = 0,
			                  int max_height = 0);

			Genode::Dataspace_capability dataspace();
			void release();
			Mode mode() const;
			void mode_sigh(Genode::Signal_context_capability sigh_cap);
			void refresh(int x, int y, int w, int h);
	};

}

#endif /* _FRAMEBUFFER_SESSION_COMPONENT_H_ */
