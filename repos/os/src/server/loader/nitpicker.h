/*
 * \brief  Virtualized nitpicker session interface exposed to the subsystem
 * \author Christian Prochaska
 * \author Norman Feske
 * \date   2009-10-05
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _NITPICKER_H_
#define _NITPICKER_H_

/* Genode includes */
#include <util/arg_string.h>
#include <util/misc_math.h>
#include <base/signal.h>
#include <os/attached_ram_dataspace.h>
#include <nitpicker_session/connection.h>
#include <nitpicker_session/nitpicker_session.h>

/* local includes */
#include <input.h>

namespace Nitpicker { class Session_component; }


class Nitpicker::Session_component : public Genode::Rpc_object<Session>
{
	private:

		/**
		 * Signal handler to be notified once the geometry of the view is
		 * known.
		 */
		Genode::Signal_context_capability _view_ready_sigh;

		Genode::Rpc_entrypoint &_ep;

		Area _max_size;

		Nitpicker::Connection _nitpicker;

		View_handle _parent_view_handle;

		/*
		 * Physical view
		 */
		View_handle _view_handle;
		Rect        _view_geometry;
		Point       _view_offset;

		/*
		 * Geometry of virtual view presented to the loaded subsystem
		 */
		Rect  _virt_view_geometry;
		Point _virt_view_offset;
		bool  _virt_view_geometry_defined = false;

		Input::Motion_delta _motion_delta;

		Input::Session_component  _proxy_input;
		Input::Session_capability _proxy_input_cap;

		static long _session_arg(const char *arg, const char *key) {
			return Genode::Arg_string::find_arg(arg, key).long_value(0); }

		/*
		 * Command buffer
		 */
		typedef Nitpicker::Session::Command_buffer Command_buffer;
		Genode::Attached_ram_dataspace _command_ds;
		Command_buffer &_command_buffer = *_command_ds.local_addr<Command_buffer>();

		void _propagate_view_offset()
		{
			_nitpicker.enqueue<Command::Offset>(_view_handle,
			                                    _view_offset + _virt_view_offset);
		}

		void _update_motion_delta()
		{
			_motion_delta = _virt_view_geometry.p1() - _view_geometry.p1();
		}

		void _execute_command(Command const &command)
		{
			switch (command.opcode) {

			case Command::OP_GEOMETRY:
				{
					_virt_view_geometry = command.geometry.rect;

					if (!_virt_view_geometry_defined)
						Genode::Signal_transmitter(_view_ready_sigh).submit();

					_virt_view_geometry_defined = true;

					_update_motion_delta();
					return;
				}

			case Command::OP_OFFSET:
				{
					_virt_view_offset = command.offset.offset;
					_propagate_view_offset();
					_nitpicker.execute();
					return;
				}

			case Command::OP_TO_FRONT:
				{
					_nitpicker.enqueue<Command::To_front>(_view_handle, _parent_view_handle);
					_nitpicker.execute();
					return;
				}

			case Command::OP_TO_BACK:
				{
					Genode::warning("OP_TO_BACK not implemented");
					return;
				}

			case Command::OP_BACKGROUND:
				{
					Genode::warning("OP_BACKGROUND not implemented");
					return;
				}

			case Command::OP_TITLE:
				{
					_nitpicker.enqueue(command);
					_nitpicker.execute();
					return;
				}

			case Command::OP_NOP:
				return;
			}
		}

	public:

		/**
		 * Constructor
		 */
		Session_component(Genode::Rpc_entrypoint            &ep,
		                  Genode::Ram_session               &ram,
		                  Area                               max_size,
		                  Nitpicker::View_capability         parent_view,
		                  Genode::Signal_context_capability  view_ready_sigh,
		                  const char                        *args)
		:
			_view_ready_sigh(view_ready_sigh),
			_ep(ep),
			_max_size(max_size),

			/* import parent view */
			_parent_view_handle(_nitpicker.view_handle(parent_view)),

			/* create nitpicker view */
			_view_handle(_nitpicker.create_view(_parent_view_handle)),

			_proxy_input(_nitpicker.input_session(), _motion_delta),
			_proxy_input_cap(_ep.manage(&_proxy_input)),

			_command_ds(&ram, sizeof(Command_buffer))
		{ }


		/*********************************
		 ** Nitpicker session interface **
		 *********************************/

		Framebuffer::Session_capability framebuffer_session() override
		{
			return _nitpicker.framebuffer_session();
		}

		Input::Session_capability input_session() override
		{
			return _proxy_input_cap;
		}

		View_handle create_view(View_handle) override
		{
			return View_handle(1);
		}

		void destroy_view(View_handle view) override { }

		View_handle view_handle(View_capability, View_handle) override
		{
			return View_handle();
		}

		View_capability view_capability(View_handle) override
		{
			return View_capability();
		}

		void release_view_handle(View_handle) override { }

		Genode::Dataspace_capability command_dataspace() override
		{
			return _command_ds.cap();
		}

		void execute() override
		{
			for (unsigned i = 0; i < _command_buffer.num(); i++)
				_execute_command(_command_buffer.get(i));
		}

		Framebuffer::Mode mode() override
		{
			int mode_width = _max_size.valid() ?
			                 _max_size.w() :
			                 _nitpicker.mode().width();

			int mode_height = _max_size.valid() ?
			                  _max_size.h() :
			                  _nitpicker.mode().height();

			return Framebuffer::Mode(mode_width, mode_height,
			                         _nitpicker.mode().format());
		}

		void mode_sigh(Genode::Signal_context_capability) override { }

		void buffer(Framebuffer::Mode mode, bool use_alpha) override
		{
			_nitpicker.buffer(mode, use_alpha);
		}

		void focus(Genode::Capability<Session>) override { }

		/**
		 * Return geometry of loader view
		 */
		Area loader_view_size() const
		{
			int const  width = _max_size.valid()
			                 ? Genode::min(_virt_view_geometry.w(), _max_size.w())
			                 : _virt_view_geometry.w();

			int const height = _max_size.valid()
			                 ? Genode::min(_virt_view_geometry.h(), _max_size.h())
			                 : _virt_view_geometry.h();

			return Area(width, height);
		}

		/**
		 * Define geometry of loader view
		 */
		void loader_view_geometry(Rect rect, Point offset)
		{
			typedef Nitpicker::Session::Command Command;

			_view_geometry = rect;
			_view_offset   = offset;

			_propagate_view_offset();
			_nitpicker.enqueue<Command::Geometry>(_view_handle, _view_geometry);
			_nitpicker.enqueue<Command::To_front>(_view_handle, _parent_view_handle);
			_nitpicker.execute();

			_update_motion_delta();
		}
};

#endif /* _NITPICKER_H_ */
