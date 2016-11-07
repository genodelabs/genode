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

/* qt_avplay includes */
#include "avplay_policy.h"
#include "filter_framebuffer_policy.h"
#include "framebuffer_root.h"
#include "main_window.h"


using namespace Genode;


struct Framebuffer_filter
{
	enum { MAX_FILTER_NAME_SIZE = 32 };
	char name[MAX_FILTER_NAME_SIZE];
	Genode::Number_of_bytes ram_quota;

	Service_registry *framebuffer_out_registry;
	Rpc_entrypoint *ep;
	Filter_framebuffer_policy *policy;
	Slave *slave;
};


Main_window::Main_window()
:
	_control_bar(_input_session.event_queue())
{
	_input_registry.insert(&_input_service);
	_ep.manage(&_input_root);

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

	/* start the filtering framebuffer services */

	Service_registry *framebuffer_in_registry = &_nitpicker_framebuffer_registry;

	Q_FOREACH(Framebuffer_filter *framebuffer_filter, framebuffer_filters) {
		framebuffer_filter->framebuffer_out_registry = new Service_registry;
		framebuffer_filter->ep = new Rpc_entrypoint(&_cap, STACK_SIZE, "filter_fb_ep");
		framebuffer_filter->policy = new Filter_framebuffer_policy(framebuffer_filter->name,
		                                                           *framebuffer_filter->ep,
		                                                           *framebuffer_in_registry,
		                                                           *framebuffer_filter->framebuffer_out_registry);
		framebuffer_filter->slave = new Slave(*framebuffer_filter->ep,
		                                      *framebuffer_filter->policy,
		                                      framebuffer_filter->ram_quota);
		framebuffer_in_registry = framebuffer_filter->framebuffer_out_registry;
	}

	Rpc_entrypoint *local_framebuffer_ep = framebuffer_filters.isEmpty() ?
	                                       &_ep :
	                                       framebuffer_filters.at(0)->ep;

	static Framebuffer::Root framebuffer_root(local_framebuffer_ep, env()->heap(), *_avplay_widget, 640, 480);
	static Local_service framebuffer_service(Framebuffer::Session::service_name(), &framebuffer_root);
	_nitpicker_framebuffer_registry.insert(&framebuffer_service);

	/* obtain dynamic linker */

	Dataspace_capability ldso_ds;
	try {
		static Rom_connection rom("ld.lib.so");
		ldso_ds = rom.dataspace();
	} catch (...) { }

	/* start avplay */

	static Avplay_policy avplay_policy(_ep, _input_registry, *framebuffer_in_registry, _mediafile_name.buf);
	static Genode::Slave avplay_slave(_ep, avplay_policy, 32*1024*1024,
	                                  env()->ram_session_cap(), ldso_ds);

	/* add widgets to layout */

	_layout->addWidget(_avplay_widget);
	_layout->addWidget(_control_bar);

	connect(_control_bar, SIGNAL(volume_changed(int)), &avplay_policy, SLOT(volume_changed(int)));
}
