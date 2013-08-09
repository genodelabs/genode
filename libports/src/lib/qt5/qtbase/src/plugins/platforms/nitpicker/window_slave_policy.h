/*
 * \brief   Slave policy for an undecorated window
 * \author  Christian Prochaska
 * \date    2013-05-08
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _UNDECORATED_WINDOW_POLICY_H_
#define _UNDECORATED_WINDOW_POLICY_H_

/* Qt4 includes */
#include <QDebug>
#include <QDomDocument>
#include <QDomElement>
#include <QDomText>

/* Genode includes */
#include <cap_session/connection.h>
#include <framebuffer_session/client.h>
#include <input/event.h>
#include <input_session/client.h>
#include <os/slave.h>

using Genode::Root_capability;
using Genode::Allocator;
using Genode::Server;
using Genode::env;
using Genode::Lock_guard;
using Genode::Lock;
using Genode::Signal_context;
using Genode::Signal_receiver;
using Genode::Signal_context_capability;
using Genode::Dataspace_capability;

static const bool wsp_verbose = false;

class Window_slave_policy : public Genode::Slave_policy
{
	private:

		Framebuffer::Session_capability  _framebuffer_session;
		Genode::Lock                     _framebuffer_ready_lock;
		unsigned char                   *_framebuffer;
		Signal_context                   _mode_change_signal_context;
		Signal_receiver                  _signal_receiver;

		Input::Session_capability        _input_session;
		Genode::Lock                     _input_ready_lock;
		Input::Event                    *_ev_buf;

		QByteArray                       _config_byte_array;

		const char *_config(int xpos, int ypos, int width, int height,
		                    const char *title, bool resize_handle = true,
		                    bool decoration = true)
		{
			QDomDocument config_doc;

			QDomElement config_node = config_doc.createElement("config");
			config_doc.appendChild(config_node);

			config_node.setAttribute("xpos", QString::number(xpos));
			config_node.setAttribute("ypos", QString::number(ypos));
			config_node.setAttribute("width", QString::number(width));
			config_node.setAttribute("height", QString::number(height));

			/* liquid_framebuffer options */
			config_node.setAttribute("title", title);
			config_node.setAttribute("animate", "off");

			if (resize_handle)
				config_node.setAttribute("resize_handle", "on");
			else
				config_node.setAttribute("resize_handle", "off");

			if (decoration)
				config_node.setAttribute("decoration", "on");
			else
				config_node.setAttribute("decoration", "off");

			_config_byte_array = config_doc.toByteArray(4);

			if (wsp_verbose)
				qDebug() << _config_byte_array;

			return _config_byte_array.constData();
		}

		void _reattach_framebuffer()
		{
			Framebuffer::Session_client session_client(_framebuffer_session);

			if (_framebuffer)
				Genode::env()->rm_session()->detach(_framebuffer);

			session_client.release();

			Dataspace_capability framebuffer_ds = session_client.dataspace();
			if (framebuffer_ds.valid()) {
				_framebuffer = Genode::env()->rm_session()->attach(framebuffer_ds);
				Framebuffer::Mode const scr_mode = session_client.mode();
				if (wsp_verbose)
					PDBG("_framebuffer = %p, width = %d, height = %d", _framebuffer, scr_mode.width(), scr_mode.height());
			}
			else
				_framebuffer = 0;
		}

	protected:

		const char **_permitted_services() const
		{
			static const char *permitted_services[] = {
				"CAP", "LOG", "RM", "ROM", "SIGNAL",
				"Timer", "Nitpicker", 0 };

			return permitted_services;
		};

	public:

		Window_slave_policy(Genode::Rpc_entrypoint &ep,
		                    int screen_width, int screen_height)
		: Genode::Slave_policy("liquid_fb", ep, env()->ram_session()),
		  _framebuffer_ready_lock(Genode::Lock::LOCKED),
		  _framebuffer(0), _ev_buf(0)
		{
			/* start with an invisible window by placing it outside of the screen area */
			Slave_policy::configure(_config(100000, 0, screen_width, screen_height, "Qt window"));
		}


		~Window_slave_policy()
		{
			if (_framebuffer)
				Genode::env()->rm_session()->detach(_framebuffer);

			if (_ev_buf)
				Genode::env()->rm_session()->detach(_ev_buf);
		}


		bool announce_service(const char            *name,
		                      Root_capability        root,
		                      Allocator             *alloc,
		                      Server                *server)
		{
			if (wsp_verbose)
				PDBG("name = %s", name);

			if (Genode::strcmp(name, "Input") == 0) {

				Genode::Session_capability session_cap =
					Genode::Root_client(root).session("ram_quota=8K",
					                                  Genode::Affinity());

				_input_session =
					Genode::static_cap_cast<Input::Session>(session_cap);

				Input::Session_client session_client(_input_session);

				_ev_buf = static_cast<Input::Event *>
				          (env()->rm_session()->attach(session_client.dataspace()));

				_input_ready_lock.unlock();

				return true;
			}

			if (Genode::strcmp(name, "Framebuffer") == 0) {

				Genode::Session_capability session_cap =
					Genode::Root_client(root).session("ram_quota=8K",
					                                  Genode::Affinity());

				_framebuffer_session =
					Genode::static_cap_cast<Framebuffer::Session>(session_cap);

				Framebuffer::Session_client session_client(_framebuffer_session);

				_framebuffer = Genode::env()->rm_session()->attach(session_client.dataspace());

				Signal_context_capability mode_change_signal_context_capability =
					_signal_receiver.manage(&_mode_change_signal_context);

				session_client.mode_sigh(mode_change_signal_context_capability);

				Framebuffer::Mode const scr_mode = session_client.mode();

				if (wsp_verbose)
					PDBG("_framebuffer = %p, width = %d, height = %d", _framebuffer, scr_mode.width(), scr_mode.height());

				_framebuffer_ready_lock.unlock();
				return true;
			}

			return Slave_policy::announce_service(name, root, alloc, server);
		}


		void wait_for_service_announcements()
		{
			Lock_guard<Lock> framebuffer_ready_lock_guard(_framebuffer_ready_lock);
			Lock_guard<Lock> input_ready_lock_guard(_input_ready_lock);
		}


		void configure(int x, int y, int width, int height, const char *title,
                       bool resize_handle, bool decoration)
		{
			Slave_policy::configure(_config(x, y, width, height, title, resize_handle, decoration));

			if (wsp_verbose)
				PDBG("waiting for mode change signal");

			_signal_receiver.wait_for_signal();

			if (wsp_verbose)
				PDBG("received mode change signal");

			_reattach_framebuffer();
		}


		/*
		 *  Return the current window size
		 */
		void size(int &width, int &height)
		{
			Framebuffer::Session_client session_client(_framebuffer_session);
			Framebuffer::Mode const scr_mode = session_client.mode();
			width = scr_mode.width();
			height = scr_mode.height();
		}


		unsigned char *framebuffer()
		{
			return _framebuffer;
		}


		void refresh(int x, int y, int w, int h)
		{
			Framebuffer::Session_client session_client(_framebuffer_session);
			session_client.refresh(x, y, w, h);
		}


		bool mode_changed()
		{
			bool result = false;

			while (_signal_receiver.pending()) {
				_signal_receiver.wait_for_signal();
				result = true;
			}

			if (result == true)
				_reattach_framebuffer();

			return result;
		}


		Input::Session_capability input_session()
		{
			return _input_session;
		}


		Input::Event *ev_buf()
		{
			return _ev_buf;
		}

};

#endif /* _UNDECORATED_WINDOW_POLICY_H_ */
