/*
 * \brief   Avplay slave
 * \author  Christian Prochaska
 * \date    2012-04-05
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _AVPLAY_SLAVE_H_
#define _AVPLAY_SLAVE_H_

/* Qt includes */
#include <QDebug>
#include <QObject>
#include <QDomDocument>
#include <QDomElement>
#include <QDomText>

/* Genode includes */
#include <input/component.h>
#include <timer_session/timer_session.h>
#include <audio_out_session/audio_out_session.h>
#include <os/static_parent_services.h>
#include <os/slave.h>

/* local includes */
#include "framebuffer_service_factory.h"

typedef Genode::Local_service<Input::Session_component> Input_service;

class Avplay_slave : public QObject
{
	Q_OBJECT

	private:

		class Policy
		:
			private Genode::Static_parent_services<Genode::Cpu_session,
			                                       Genode::Log_session,
			                                       Genode::Pd_session,
			                                       Genode::Rom_session,
			                                       Timer::Session,
			                                       Audio_out::Session>,
			public Genode::Slave::Policy
		{
			private:

				Input_service               &_input_service;
				Framebuffer_service_factory &_framebuffer_service_factory;

				const char *_mediafile;
				int _sdl_audio_volume;
				QByteArray _config_byte_array;


				const char *_config()
				{
					QDomDocument config_doc;

					QDomElement config_node = config_doc.createElement("config");
					config_doc.appendChild(config_node);

					QDomElement arg0_node = config_doc.createElement("arg");
					arg0_node.setAttribute("value", "avplay");
					config_node.appendChild(arg0_node);

					QDomElement arg1_node = config_doc.createElement("arg");
					arg1_node.setAttribute("value", _mediafile);
					config_node.appendChild(arg1_node);

					/*
			 	 	 * Configure libc of avplay to direct output to LOG and to obtain
			 	 	 * the mediafile from ROM.
			 	 	 */

					QDomElement vfs_node = config_doc.createElement("vfs");
					QDomElement vfs_dev_node = config_doc.createElement("dir");
					vfs_dev_node.setAttribute("name", "dev");
					QDomElement vfs_dev_log_node = config_doc.createElement("log");
					vfs_dev_node.appendChild(vfs_dev_log_node);
					vfs_node.appendChild(vfs_dev_node);
					QDomElement vfs_mediafile_node = config_doc.createElement("rom");
					vfs_mediafile_node.setAttribute("name", "mediafile");
					vfs_node.appendChild(vfs_mediafile_node);
					config_node.appendChild(vfs_node);

					QDomElement libc_node = config_doc.createElement("libc");
					libc_node.setAttribute("stdout", "/dev/log");
					libc_node.setAttribute("stderr", "/dev/log");
					config_node.appendChild(libc_node);

					QDomElement sdl_audio_volume_node = config_doc.createElement("sdl_audio_volume");
					sdl_audio_volume_node.setAttribute("value", QString::number(_sdl_audio_volume));
					config_node.appendChild(sdl_audio_volume_node);

					_config_byte_array = config_doc.toByteArray(4);

					return _config_byte_array.constData();
				}

				static Genode::Cap_quota _caps()      { return { 150 }; }
				static Genode::Ram_quota _ram_quota() { return { 32*1024*1024 }; }
				static Name              _name()      { return "avplay"; }

			public:

				Policy(Genode::Rpc_entrypoint         &entrypoint,
				       Genode::Region_map             &rm,
				       Genode::Pd_session             &ref_pd,
				       Genode::Pd_session_capability   ref_pd_cap,
				       Input_service                  &input_service,
				       Framebuffer_service_factory    &framebuffer_service_factory,
				       char const                     *mediafile)
				:
					Genode::Slave::Policy(_name(), _name(), *this, entrypoint,
					                      rm, ref_pd, ref_pd_cap, _caps(),
					                      _ram_quota()),
					_input_service(input_service),
					_framebuffer_service_factory(framebuffer_service_factory),
					_mediafile(mediafile),
					_sdl_audio_volume(100)
				{
					configure(_config());
				}

				Genode::Service &resolve_session_request(Genode::Service::Name       const &service_name,
		                                         	 	 Genode::Session_state::Args const &args) override
				{
					if (service_name == "Input")
						return _input_service;

					if (service_name == "Framebuffer")
						return _framebuffer_service_factory.create(args);

					return Genode::Slave::Policy::resolve_session_request(service_name, args);
				}

				void volume_changed(int value)
				{
					_sdl_audio_volume = value;
					configure(_config());
				}

		};

		Genode::size_t   const _ep_stack_size = 4*1024*sizeof(Genode::addr_t);
		Genode::Rpc_entrypoint _ep;
		Policy                 _policy;
		Genode::Child          _child;

	public:

		/**
		 * Constructor
		 */
		Avplay_slave(Genode::Region_map             &rm,
		             Genode::Pd_session             &ref_pd,
		             Genode::Pd_session_capability   ref_pd_cap,
		             Input_service                  &input_service,
		             Framebuffer_service_factory    &framebuffer_service_factory,
		             char const                     *mediafile)
		:
			_ep(&ref_pd, _ep_stack_size, "avplay_ep"),
			_policy(_ep, rm, ref_pd, ref_pd_cap, input_service,
			        framebuffer_service_factory, mediafile),
			_child(rm, _ep, _policy)
		{ }

	public Q_SLOTS:

		void volume_changed(int value)
		{
			_policy.volume_changed(value);
		}
};

#endif /* _AVPLAY_SLAVE_H_ */
