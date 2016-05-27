/*
 * \brief   Avplay policy
 * \author  Christian Prochaska
 * \date    2012-04-05
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _AVPLAY_POLICY_H_
#define _AVPLAY_POLICY_H_

/* Qt includes */
#include <QDebug>
#include <QObject>
#include <QDomDocument>
#include <QDomElement>
#include <QDomText>

/* Genode includes */
#include <os/slave.h>


class Avplay_policy : public QObject, public Genode::Slave_policy
{
	Q_OBJECT

	private:

		Genode::Service_registry &_input_in;
		Genode::Service_registry &_framebuffer_in;

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
			QDomElement libc_node = config_doc.createElement("libc");
			libc_node.setAttribute("stdout", "/dev/log");
			libc_node.setAttribute("stderr", "/dev/log");
			QDomElement libc_vfs_node = config_doc.createElement("vfs");
			QDomElement libc_vfs_dev_node = config_doc.createElement("dir");
			libc_vfs_dev_node.setAttribute("name", "dev");
			QDomElement libc_vfs_dev_log_node = config_doc.createElement("log");
			libc_vfs_dev_node.appendChild(libc_vfs_dev_log_node);
			libc_vfs_node.appendChild(libc_vfs_dev_node);
			QDomElement libc_vfs_mediafile_node = config_doc.createElement("rom");
			libc_vfs_mediafile_node.setAttribute("name", "mediafile");
			libc_vfs_node.appendChild(libc_vfs_mediafile_node);
			libc_node.appendChild(libc_vfs_node);
			config_node.appendChild(libc_node);

			QDomElement sdl_audio_volume_node = config_doc.createElement("sdl_audio_volume");
			sdl_audio_volume_node.setAttribute("value", QString::number(_sdl_audio_volume));
			config_node.appendChild(sdl_audio_volume_node);

			_config_byte_array = config_doc.toByteArray(4);

			return _config_byte_array.constData();
		}

	protected:

		const char **_permitted_services() const
		{
			static const char *permitted_services[] = {
				"LOG", "RM", "ROM", "Timer", "Audio_out", 0 };

			return permitted_services;
		};

	public:

		Avplay_policy(Genode::Rpc_entrypoint &entrypoint,
		              Genode::Service_registry &input_in,
		              Genode::Service_registry &framebuffer_in,
		              const char *mediafile)
		: Genode::Slave_policy("avplay", entrypoint, Genode::env()->ram_session()),
		  _input_in(input_in),
		  _framebuffer_in(framebuffer_in),
		  _mediafile(mediafile),
		  _sdl_audio_volume(100)
		{
			configure(_config());
		}

		Genode::Service *resolve_session_request(const char *service_name,
		                                         const char *args)
		{
			if (strcmp(service_name, "Input") == 0)
				return _input_in.find(service_name);

			if (strcmp(service_name, "Framebuffer") == 0) {
				Genode::Client client;
				return _framebuffer_in.wait_for_service(service_name, &client, name());
			}

			return Slave_policy::resolve_session_request(service_name, args);
		}

	public Q_SLOTS:

		void volume_changed(int value)
		{
			_sdl_audio_volume = value;
			configure(_config());
		}
};

#endif /* _AVPLAY_POLICY_H_ */
