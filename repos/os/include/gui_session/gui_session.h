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
		enum Opcode { OP_GEOMETRY, OP_OFFSET,
		              OP_TO_FRONT, OP_TO_BACK, OP_BACKGROUND,
		              OP_TITLE, OP_NOP };

		struct Nop { static Opcode opcode() { return OP_NOP; } };

		struct Geometry
		{
			static Opcode opcode() { return OP_GEOMETRY; }
			View_handle view;
			Rect        rect;
		};

		struct Offset
		{
			static Opcode opcode() { return OP_OFFSET; }
			View_handle view;
			Point       offset;
		};

		struct To_front
		{
			static Opcode opcode() { return OP_TO_FRONT; }
			View_handle view;
			View_handle neighbor;
		};

		struct To_back
		{
			static Opcode opcode() { return OP_TO_BACK; }
			View_handle view;
			View_handle neighbor;
		};

		struct Background
		{
			static Opcode opcode() { return OP_BACKGROUND; }
			View_handle view;
		};

		struct Title
		{
			static Opcode opcode() { return OP_TITLE; }
			View_handle view;
			String<64> title;
		};

		Opcode opcode;
		union
		{
			Nop        nop;
			Geometry   geometry;
			Offset     offset;
			To_front   to_front;
			To_back    to_back;
			Background background;
			Title      title;
		};

		Command() : opcode(OP_NOP) { }

		template <typename ARGS>
		Command(ARGS args) : opcode(ARGS::opcode())
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
	 * Request framebuffer sub-session
	 */
	virtual Framebuffer::Session_capability framebuffer_session() = 0;

	/**
	 * Request input sub-session
	 */
	virtual Input::Session_capability input_session() = 0;

	/**
	 * Create a new top-level view at the buffer
	 *
	 * \throw   Invalid_handle
	 * \return  handle for new view
	 */
	virtual View_handle create_view() = 0;

	/**
	 * Create a new child view at the buffer
	 *
	 * \param parent  parent view
	 *
	 * \throw   Invalid_handle
	 * \return  handle for new view
	 *
	 * The 'parent' argument allows the client to use the location of an
	 * existing view as the coordinate origin for the to-be-created view.
	 */
	virtual View_handle create_child_view(View_handle parent) = 0;

	/**
	 * Destroy view
	 */
	virtual void destroy_view(View_handle) = 0;

	/**
	 * Return session-local handle for the specified view
	 *
	 * The handle returned by this method can be used to issue commands
	 * via the 'execute' method.
	 *
	 * \param handle  designated view handle to be assigned to the imported
	 *                view. By default, a new handle will be allocated.
	 *
	 * \throw Out_of_ram
	 * \throw Out_of_caps
	 */
	virtual View_handle view_handle(View_capability,
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

	/**
	 * Define dimensions of virtual framebuffer
	 *
	 * \throw Out_of_ram  session quota does not suffice for specified
	 *                    buffer dimensions
	 * \throw Out_of_caps
	 */
	virtual void buffer(Framebuffer::Mode mode, bool use_alpha) = 0;

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

	GENODE_RPC(Rpc_framebuffer_session, Framebuffer::Session_capability, framebuffer_session);
	GENODE_RPC(Rpc_input_session, Input::Session_capability, input_session);
	GENODE_RPC_THROW(Rpc_create_view, View_handle, create_view,
	                 GENODE_TYPE_LIST(Out_of_ram, Out_of_caps));
	GENODE_RPC_THROW(Rpc_create_child_view, View_handle, create_child_view,
	                 GENODE_TYPE_LIST(Out_of_ram, Out_of_caps, Invalid_handle), View_handle);
	GENODE_RPC(Rpc_destroy_view, void, destroy_view, View_handle);
	GENODE_RPC_THROW(Rpc_view_handle, View_handle, view_handle,
	                 GENODE_TYPE_LIST(Out_of_ram, Out_of_caps), View_capability, View_handle);
	GENODE_RPC(Rpc_view_capability, View_capability, view_capability, View_handle);
	GENODE_RPC(Rpc_release_view_handle, void, release_view_handle, View_handle);
	GENODE_RPC(Rpc_command_dataspace, Dataspace_capability, command_dataspace);
	GENODE_RPC(Rpc_execute, void, execute);
	GENODE_RPC(Rpc_background, int, background, View_capability);
	GENODE_RPC(Rpc_mode, Framebuffer::Mode, mode);
	GENODE_RPC(Rpc_mode_sigh, void, mode_sigh, Signal_context_capability);
	GENODE_RPC(Rpc_focus, void, focus, Capability<Session>);
	GENODE_RPC_THROW(Rpc_buffer, void, buffer, GENODE_TYPE_LIST(Out_of_ram, Out_of_caps),
	                 Framebuffer::Mode, bool);

	GENODE_RPC_INTERFACE(Rpc_framebuffer_session, Rpc_input_session,
	                     Rpc_create_view, Rpc_create_child_view, Rpc_destroy_view,
	                     Rpc_view_handle, Rpc_view_capability, Rpc_release_view_handle,
	                     Rpc_command_dataspace, Rpc_execute, Rpc_mode,
	                     Rpc_mode_sigh, Rpc_buffer, Rpc_focus);
};

#endif /* _INCLUDE__GUI_SESSION__GUI_SESSION_H_ */
