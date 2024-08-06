/*
 * \brief  GUI session interface
 * \author Norman Feske
 * \date   2006-08-10
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__GUI_SESSION__GUI_SESSION_H_
#define _INCLUDE__GUI_SESSION__GUI_SESSION_H_

#include <session/session.h>
#include <os/surface.h>
#include <os/handle_registry.h>
#include <framebuffer_session/capability.h>
#include <input_session/capability.h>

namespace Gui {

	using namespace Genode;

	struct Session_client;
	struct View;
	struct Session;

	using View_capability = Capability<View>;

	using Rect  = Surface_base::Rect;
	using Point = Surface_base::Point;
	using Area  = Surface_base::Area;
}


struct Gui::Session : Genode::Session
{
	/**
	 * \noapi
	 */
	static const char *service_name() { return "Gui"; }

	/*
	 * A nitpicker session consumes a dataspace capability for the server's
	 * session-object allocation, a session capability, a dataspace capability
	 * for the command buffer, and the capabilities needed for the aggregated
	 * 'Framebuffer' and 'Input' sessions.
	 */
	static constexpr unsigned CAP_QUOTA = Framebuffer::Session::CAP_QUOTA
	                                    + Input::Session::CAP_QUOTA + 3;

	/**
	 * Session-local view handle
	 *
	 * When issuing commands to nitpicker via the 'execute' method, views
	 * are referenced by session-local handles.
	 */
	using View_handle = Handle<View>;


	struct Command
	{
		enum Opcode { GEOMETRY, OFFSET, FRONT, BACK, FRONT_OF, BEHIND_OF,
		              BACKGROUND, TITLE, NOP };

		struct Nop { static constexpr Opcode opcode = NOP; };

		/**
		 * Operation that takes a view as first argument
		 */
		template <Opcode OPCODE>
		struct View_op
		{
			static constexpr Opcode opcode = OPCODE;
			View_handle view;
		};

		struct Geometry   : View_op<GEOMETRY>   { Rect rect; };
		struct Offset     : View_op<OFFSET>     { Point offset; };
		struct Front      : View_op<FRONT>      { };
		struct Back       : View_op<BACK>       { };
		struct Front_of   : View_op<FRONT_OF>   { View_handle neighbor; };
		struct Behind_of  : View_op<BEHIND_OF>  { View_handle neighbor; };
		struct Background : View_op<BACKGROUND> { };
		struct Title      : View_op<TITLE>      { String<64> title; };

		Opcode opcode;
		union
		{
			Nop        nop;
			Geometry   geometry;
			Offset     offset;
			Front      front;
			Back       back;
			Front_of   front_of;
			Behind_of  behind_of;
			Background background;
			Title      title;
		};

		Command() : opcode(Opcode::NOP) { }

		template <typename ARGS>
		Command(ARGS args) : opcode(ARGS::opcode)
		{
			reinterpret_cast<ARGS &>(nop) = args;
		}
	};


	/**
	 * Command buffer shared between nitpicker and client
	 */
	class Command_buffer
	{
		public:

			enum { MAX_COMMANDS = 64 };

		private:

			unsigned _num = 0;

			Command _commands[MAX_COMMANDS];

		public:

			bool full() const { return _num >= MAX_COMMANDS; }

			unsigned num() const
			{
				/* copy out _num value to avoid use-after-check problems */
				unsigned const num = _num;
				return num <= MAX_COMMANDS ? num : 0;
			}

			void reset() { _num = 0; }

			/**
			 * Enqueue command
			 *
			 * The command will be dropped if the buffer is full. Check for this
			 * condition by calling 'full()' prior calling this method.
			 */
			void enqueue(Command const &command)
			{
				if (!full())
					_commands[_num++] = command;
			}

			Command get(unsigned i)
			{
				return (i < MAX_COMMANDS) ? _commands[i] : Command(Command::Nop());
			}
	};

	/**
	 * Exception types
	 */
	struct Invalid_handle  : Exception { };

	virtual ~Session() { }

	/**
	 * Request framebuffer RPC interface
	 */
	virtual Framebuffer::Session_capability framebuffer() = 0;

	/**
	 * Request input RPC interface
	 */
	virtual Input::Session_capability input() = 0;

	enum class Create_view_error { OUT_OF_RAM, OUT_OF_CAPS };
	using      Create_view_result = Attempt<View_handle, Create_view_error>;

	/**
	 * Create a new top-level view at the buffer
	 */
	virtual Create_view_result create_view() = 0;

	enum class Create_child_view_error { OUT_OF_RAM, OUT_OF_CAPS, INVALID };
	using      Create_child_view_result = Attempt<View_handle, Create_child_view_error>;

	/**
	 * Create a new child view at the buffer
	 *
	 * \param   parent  parent view
	 * \return  handle for new view
	 *
	 * The 'parent' argument allows the client to use the location of an
	 * existing view as the coordinate origin for the to-be-created view.
	 */
	virtual Create_child_view_result create_child_view(View_handle parent) = 0;

	/**
	 * Destroy view
	 */
	virtual void destroy_view(View_handle) = 0;

	enum class View_handle_error { OUT_OF_RAM, OUT_OF_CAPS };
	using      View_handle_result = Attempt<View_handle, View_handle_error>;

	/**
	 * Return session-local handle for the specified view
	 *
	 * The handle returned by this method can be used to issue commands
	 * via the 'execute' method.
	 *
	 * \param handle  designated view handle to be assigned to the imported
	 *                view. By default, a new handle will be allocated.
	 */
	virtual View_handle_result view_handle(View_capability,
	                                       View_handle handle = View_handle()) = 0;

	/**
	 * Request view capability for a given handle
	 *
	 * The returned view capability can be used to share the view with another
	 * session.
	 */
	virtual View_capability view_capability(View_handle) = 0;

	/**
	 * Release session-local view handle
	 */
	virtual void release_view_handle(View_handle) = 0;

	/**
	 * Request dataspace used for issuing view commands to nitpicker
	 */
	virtual Dataspace_capability command_dataspace() = 0;

	/**
	 * Execution batch of commands contained in the command dataspace
	 */
	virtual void execute() = 0;

	/**
	 * Return physical screen mode
	 */
	virtual Framebuffer::Mode mode() = 0;

	/**
	 * Register signal handler to be notified about mode changes
	 */
	virtual void mode_sigh(Signal_context_capability) = 0;

	enum class Buffer_result { OK, OUT_OF_RAM, OUT_OF_CAPS };

	/**
	 * Define dimensions of virtual framebuffer
	 */
	virtual Buffer_result buffer(Framebuffer::Mode mode, bool use_alpha) = 0;

	/**
	 * Set focused session
	 *
	 * Normally, the focused session is defined by the 'focus' ROM, which is
	 * driven by an external policy component. However, in cases where one
	 * application consists of multiple nitpicker sessions, it is desirable to
	 * let the application manage the focus among its sessions by applying an
	 * application-local policy. The 'focus' RPC function allows a client to
	 * specify another client that should receive the focus whenever the
	 * session becomes focused. As the designated receiver of the focus is
	 * referred to by its session capability, a common parent can manage the
	 * focus among its children. But unrelated sessions cannot interfere.
	 */
	virtual void focus(Capability<Session> focused) = 0;

	/**
	 * Return number of bytes needed for virtual framebuffer of specified size
	 */
	static size_t ram_quota(Framebuffer::Mode mode, bool use_alpha)
	{
		/*
		 * If alpha blending is used, each pixel requires an additional byte
		 * for the alpha value and a byte holding the input mask.
		 */
		return (mode.bytes_per_pixel() + 2*use_alpha)*mode.area.count();
	}


	/*********************
	 ** RPC declaration **
	 *********************/

	GENODE_RPC(Rpc_framebuffer, Framebuffer::Session_capability, framebuffer);
	GENODE_RPC(Rpc_input, Input::Session_capability, input);
	GENODE_RPC(Rpc_create_view, Create_view_result, create_view);
	GENODE_RPC(Rpc_create_child_view, Create_child_view_result, create_child_view, View_handle);
	GENODE_RPC(Rpc_destroy_view, void, destroy_view, View_handle);
	GENODE_RPC(Rpc_view_handle, View_handle_result, view_handle, View_capability, View_handle);
	GENODE_RPC(Rpc_view_capability, View_capability, view_capability, View_handle);
	GENODE_RPC(Rpc_release_view_handle, void, release_view_handle, View_handle);
	GENODE_RPC(Rpc_command_dataspace, Dataspace_capability, command_dataspace);
	GENODE_RPC(Rpc_execute, void, execute);
	GENODE_RPC(Rpc_background, int, background, View_capability);
	GENODE_RPC(Rpc_mode, Framebuffer::Mode, mode);
	GENODE_RPC(Rpc_mode_sigh, void, mode_sigh, Signal_context_capability);
	GENODE_RPC(Rpc_focus, void, focus, Capability<Session>);
	GENODE_RPC(Rpc_buffer, Buffer_result, buffer, Framebuffer::Mode, bool);

	GENODE_RPC_INTERFACE(Rpc_framebuffer, Rpc_input,
	                     Rpc_create_view, Rpc_create_child_view, Rpc_destroy_view,
	                     Rpc_view_handle, Rpc_view_capability, Rpc_release_view_handle,
	                     Rpc_command_dataspace, Rpc_execute, Rpc_mode,
	                     Rpc_mode_sigh, Rpc_buffer, Rpc_focus);
};

#endif /* _INCLUDE__GUI_SESSION__GUI_SESSION_H_ */
