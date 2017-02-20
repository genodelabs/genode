/*
 * \brief   Main window of the media player
 * \author  Christian Prochaska
 * \date    2012-03-29
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* qt_avplay includes */
#include "filter_framebuffer_slave.h"
#include "main_window.h"


using namespace Genode;


struct Framebuffer_filter
{
	enum { MAX_FILTER_NAME_SIZE = 32 };
	char                      name[MAX_FILTER_NAME_SIZE];
	Genode::Number_of_bytes   ram_quota;
	Filter_framebuffer_slave *slave;
};


Main_window::Main_window(Genode::Env &env)
:
	_env(env),
	_control_bar(_input_session_component)
{
	_input_session_component.event_queue().enabled(true);
	_ep.manage(&_input_session_component);

	/* add widgets to layout */

	_layout->addWidget(_avplay_widget);
	_layout->addWidget(_control_bar);

	/*
	 * The main window must be visible before avplay or a framebuffer filter
	 * requests the framebuffer session which goes to Nitpicker, because the
	 * parent view of the new Nitpicker view is part of the
	 * QNitpickerPlatformWindow object, which is created when the main window
	 * becomes visible.
	 */

	show();

	/* find out which filtering framebuffer services to start and sort them in reverse order */

	static QList<Framebuffer_filter*> framebuffer_filters;
	try {
		Xml_node node = config()->xml_node().sub_node("framebuffer_filter");
		for (; ; node = node.next("framebuffer_filter")) {
			Framebuffer_filter *framebuffer_filter = new Framebuffer_filter;
			node.attribute("name").value(framebuffer_filter->name, sizeof(framebuffer_filter->name));
			node.attribute("ram_quota").value(&framebuffer_filter->ram_quota);
			qDebug() << "filter:" << framebuffer_filter->name << "," << framebuffer_filter->ram_quota;
			framebuffer_filters.prepend(framebuffer_filter);
		}
	} catch (Xml_node::Nonexistent_sub_node) { }

	Framebuffer_service_factory *framebuffer_service_factory =
		&_nitpicker_framebuffer_service_factory;

	/* start the filtering framebuffer services */

	Q_FOREACH(Framebuffer_filter *framebuffer_filter, framebuffer_filters) {
		framebuffer_filter->slave = new Filter_framebuffer_slave(_env.pd(), _env.rm(),
		                                                         _env.ram_session_cap(),
		                                                         framebuffer_filter->name,
		                                                         framebuffer_filter->ram_quota,
		                                                         *framebuffer_service_factory);
		framebuffer_service_factory =
			new Filter_framebuffer_service_factory(framebuffer_filter->slave->policy());
	}

	/* start avplay */

	Avplay_slave *avplay_slave = new Avplay_slave(_env.pd(), _env.rm(),
	                                              _env.ram_session_cap(),
	                                              _input_service,
	                                              *framebuffer_service_factory,
	                                              _mediafile_name.buf);

	connect(_control_bar, SIGNAL(volume_changed(int)), avplay_slave, SLOT(volume_changed(int)));
}
