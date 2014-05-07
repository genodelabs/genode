/*
 * \brief   Framebuffer session component
 * \author  Christian Prochaska
 * \date    2012-04-02
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
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

			Genode::Dataspace_capability dataspace() override;
			Mode mode() const override;
			void mode_sigh(Genode::Signal_context_capability) override;
			void sync_sigh(Genode::Signal_context_capability) override;
			void refresh(int, int, int, int) override;
	};

}

#endif /* _FRAMEBUFFER_SESSION_COMPONENT_H_ */
