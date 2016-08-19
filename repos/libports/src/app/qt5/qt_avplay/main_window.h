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
#include "control_bar.h"


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
		} _mediafile_name;

		enum { STACK_SIZE = 2*sizeof(Genode::addr_t)*1024 };
		Genode::Cap_connection _cap;
		Genode::Rpc_entrypoint _ep { &_cap, STACK_SIZE, "avplay_ep" };
		Genode::Service_registry _input_registry;
		Genode::Service_registry _nitpicker_framebuffer_registry;

		Input::Session_component _input_session;
		Input::Root_component    _input_root { _ep, _input_session };

		Genode::Local_service _input_service { Input::Session::service_name(), &_input_root };

		QMember<QNitpickerViewWidget> _avplay_widget;
		QMember<Control_bar>          _control_bar;

	public:

		Main_window();
};

#endif /* _MAIN_WINDOW_H_ */
