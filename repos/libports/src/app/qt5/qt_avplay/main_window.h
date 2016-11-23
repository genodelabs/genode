/*
 * \brief   Main window of the media player
 * \author  Christian Prochaska
 * \date    2012-03-29
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _MAIN_WINDOW_H_
#define _MAIN_WINDOW_H_

/* Qt includes */
#include <QVBoxLayout>
#include <QWidget>
#include <qnitpickerviewwidget/qnitpickerviewwidget.h>

/* Qoost includes */
#include <qoost/compound_widget.h>
#include <qoost/qmember.h>

/* Genode includes */
#include <base/service.h>
#include <cap_session/connection.h>
#include <input/root.h>
#include <os/config.h>
#include <rom_session/connection.h>

/* local includes */
#include "avplay_slave.h"
#include "control_bar.h"
#include "framebuffer_service_factory.h"

class Main_window : public Compound_widget<QWidget, QVBoxLayout>
{
	Q_OBJECT

	private:

		struct Mediafile_name
		{
			/* get the name of the media file from the config file */
			enum { MAX_LEN_MEDIAFILE_NAME = 256 };
			char buf[MAX_LEN_MEDIAFILE_NAME];

			Mediafile_name()
			{
				Genode::strncpy(buf, "mediafile", sizeof(buf));
				try {
					Genode::config()->xml_node().sub_node("mediafile")
						.attribute("name").value(buf, sizeof(buf));
				} catch(...) {
					Genode::warning("no <mediafile> config node found, using \"mediafile\"");
				}
			}
		};

		Genode::Env                          &_env;

		Mediafile_name                        _mediafile_name;

		QMember<QNitpickerViewWidget>         _avplay_widget;
		QMember<Control_bar>                  _control_bar;

		Genode::size_t const                  _ep_stack_size { 2*sizeof(Genode::addr_t)*1024 };
		Genode::Rpc_entrypoint                _ep { &_env.pd(), _ep_stack_size, "avplay_ep" };

		Nitpicker_framebuffer_service_factory _nitpicker_framebuffer_service_factory { _env,
		                                                                               *_avplay_widget,
		                                                                               640, 480 };

		Input::Session_component              _input_session_component { _env, _env.ram() };
		Input_service::Single_session_factory _input_factory { _input_session_component };
		Input_service                         _input_service { _input_factory };

	public:

		Main_window(Genode::Env &env);
};

#endif /* _MAIN_WINDOW_H_ */
