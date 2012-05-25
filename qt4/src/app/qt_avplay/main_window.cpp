/*
 * \brief   Main window of the media player
 * \author  Christian Prochaska
 * \date    2012-03-29
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <cap_session/connection.h>
#include <input/component.h>
#include <os/config.h>
#include <rom_session/connection.h>

/* qt_avplay includes */
#include "avplay_policy.h"
#include "filter_framebuffer_policy.h"
#include "framebuffer_root.h"
#include "input_service.h"
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
{
	/* look for dynamic linker */

	try {
		static Rom_connection ldso_rom("ld.lib.so");
		Process::dynamic_linker(ldso_rom.dataspace());
	} catch (...) {
		PERR("ld.lib.so not found");
	}

	/* get the name of the media file from the config file */
	enum { MAX_LEN_MEDIAFILE_NAME = 256 };
	static char mediafile[MAX_LEN_MEDIAFILE_NAME] = "mediafile";
	try {
		config()->xml_node().sub_node("mediafile").attribute("name").value(mediafile, sizeof(mediafile));
	} catch(...) {
		PWRN("no <mediafile> config node found, using \"mediafile\"");
	}

	/* create local services */

	enum { STACK_SIZE = 2*sizeof(addr_t)*1024 };
	static Cap_connection cap;
	static Rpc_entrypoint avplay_ep(&cap, STACK_SIZE, "avplay_ep");
	static Service_registry input_registry;
	static Service_registry nitpicker_framebuffer_registry;

	static Input::Root input_root(&avplay_ep, env()->heap());
	static Local_service input_service(Input::Session::service_name(), &input_root);
	input_registry.insert(&input_service);
	avplay_ep.manage(&input_root);

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
	} catch (Config::Invalid) {
	} catch (Xml_node::Nonexistent_sub_node) {
	}

	/* start the filtering framebuffer services */

	Service_registry *framebuffer_in_registry = &nitpicker_framebuffer_registry;

	Q_FOREACH(Framebuffer_filter *framebuffer_filter, framebuffer_filters) {
		framebuffer_filter->framebuffer_out_registry = new Service_registry;
		framebuffer_filter->ep = new Rpc_entrypoint(&cap, STACK_SIZE, "filter_fb_ep");
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
	                                       &avplay_ep :
	                                       framebuffer_filters.at(0)->ep;

	static Framebuffer::Root framebuffer_root(local_framebuffer_ep, env()->heap(), *_avplay_widget, 640, 480);
	static Local_service framebuffer_service(Framebuffer::Session::service_name(), &framebuffer_root);
	nitpicker_framebuffer_registry.insert(&framebuffer_service);

	/* start avplay */

	static Avplay_policy avplay_policy(avplay_ep, input_registry, *framebuffer_in_registry, mediafile);
	static Genode::Slave avplay_slave(avplay_ep, avplay_policy, 32*1024*1024);

	/* add widgets to layout */

	_layout->addWidget(_avplay_widget);
	_layout->addWidget(_control_bar);

	connect(_control_bar, SIGNAL(volume_changed(int)), &avplay_policy, SLOT(volume_changed(int)));
}
