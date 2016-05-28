/*
 * \brief  Nitpicker session interface
 * \author Norman Feske
 * \date   2006-08-10
 *
 * A Nitpicker session handles exactly one buffer.
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__NITPICKER_SESSION__NITPICKER_SESSION_H_
#define _INCLUDE__NITPICKER_SESSION__NITPICKER_SESSION_H_

#include <session/session.h>
#include <os/surface.h>
#include <os/handle_registry.h>
#include <framebuffer_session/capability.h>
#include <input_session/capability.h>

namespace Nitpicker {
	using Genode::size_t;
	struct View;
	typedef Genode::Capability<View> View_capability;
	struct Session;
	typedef Genode::Surface_base::Rect  Rect;
	typedef Genode::Surface_base::Point Point;
	typedef Genode::Surface_base::Area  Area;
}


struct Nitpicker::Session : Genode::Session
{
	static const char *service_name() { return "Nitpicker"; }

	/**
	 * Session-local view handle
	 *
	 * When issuing commands to nitpicker via the 'execute' method, views
	 * are referenced by session-local handles.
	 */
	typedef Genode::Handle<View> View_handle;


	struct Command
	{
		enum Opcode { OP_GEOMETRY, OP_OFFSET,
		              OP_TO_FRONT, OP_TO_BACK, OP_BACKGROUND,
		              OP_TITLE, OP_NOP };

		/*
		 * Argument structures for nitpicker's command interface
		 */

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
			Genode::String<64> title;
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
		Command(ARGS args)
		{
			opcode = ARGS::opcode();
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

			/**
			 * Return true if there is no space left in the command buffer
			 *
			 * \noapi
			 * \deprecated  use 'full' instead
			 */
			bool is_full() const { return full(); }

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
				if (i >= MAX_COMMANDS) return Command(Command::Nop());

				return _commands[i];
			}

	};

	/**
	 * Exception types
	 */
	struct Out_of_metadata : Genode::Exception { };
	struct Invalid_handle  : Genode::Exception { };

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
	 * Create a new view at the buffer
	 *
	 * \param parent  parent view
	 *
	 * \throw   Invalid_handle
	 * \return  handle for new view
	 *
	 * The 'parent' argument allows the client to use the location of an
	 * existing view as the coordinate origin for the to-be-created view.
	 * If an invalid handle is specified (default), the view will be a
	 * top-level view.
	 */
	virtual View_handle create_view(View_handle parent = View_handle()) = 0;

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
	 * \throw Out_of_metadata
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
	virtual Genode::Dataspace_capability command_dataspace() = 0;

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
	virtual void mode_sigh(Genode::Signal_context_capability) = 0;

	/**
	 * Define dimensions of virtual framebuffer
	 *
	 * \throw Out_of_metadata  session quota does not suffice for specified
	 *                         buffer dimensions
	 */
	virtual void buffer(Framebuffer::Mode mode, bool use_alpha) = 0;

	/**
	 * Set focused session
	 *
	 * Normally, the focused session is defined by the user by clicking on a
	 * view. The 'focus' method allows a client to set the focus without user
	 * action. However, the change of the focus is performed only is the
	 * currently focused session belongs to a child or the same process as the
	 * called session. This relationship is checked by comparing the session
	 * labels of the currently focused session and the caller. This way, a
	 * common parent can manage the focus among its child processes. But a
	 * session cannot steal the focus from an unrelated session.
	 */
	virtual void focus(Genode::Capability<Session> focused) = 0;

	typedef Genode::String<160> Label;

	enum Session_control { SESSION_CONTROL_HIDE, SESSION_CONTROL_SHOW,
		                   SESSION_CONTROL_TO_FRONT };

	/**
	 * Perform control operation on one or multiple sessions
	 *
	 * The 'label' is used to select the sessions, on which the 'operation' is
	 * performed. Nitpicker creates a selector string by concatenating the
	 * caller's session label with the supplied 'label' argument. A session is
	 * selected if its label starts with the selector string. Thereby, the
	 * operation is limited to the caller session or any child session of the
	 * caller.
	 */
	virtual void session_control(Label label, Session_control operation) { }

	/**
	 * Return number of bytes needed for virtual framebuffer of specified size
	 */
	static size_t ram_quota(Framebuffer::Mode mode, bool use_alpha)
	{
		/*
		 * If alpha blending is used, each pixel requires an additional byte
		 * for the alpha value and a byte holding the input mask.
		 */
		return (mode.bytes_per_pixel() + 2*use_alpha)*mode.width()*mode.height();
	}


	/*********************
	 ** RPC declaration **
	 *********************/

	GENODE_RPC(Rpc_framebuffer_session, Framebuffer::Session_capability, framebuffer_session);
	GENODE_RPC(Rpc_input_session, Input::Session_capability, input_session);
	GENODE_RPC_THROW(Rpc_create_view, View_handle, create_view,
	                 GENODE_TYPE_LIST(Out_of_metadata, Invalid_handle), View_handle);
	GENODE_RPC(Rpc_destroy_view, void, destroy_view, View_handle);
	GENODE_RPC_THROW(Rpc_view_handle, View_handle, view_handle,
	                 GENODE_TYPE_LIST(Out_of_metadata), View_capability, View_handle);
	GENODE_RPC(Rpc_view_capability, View_capability, view_capability, View_handle);
	GENODE_RPC(Rpc_release_view_handle, void, release_view_handle, View_handle);
	GENODE_RPC(Rpc_command_dataspace, Genode::Dataspace_capability, command_dataspace);
	GENODE_RPC(Rpc_execute, void, execute);
	GENODE_RPC(Rpc_background, int, background, View_capability);
	GENODE_RPC(Rpc_mode, Framebuffer::Mode, mode);
	GENODE_RPC(Rpc_mode_sigh, void, mode_sigh, Genode::Signal_context_capability);
	GENODE_RPC(Rpc_focus, void, focus, Genode::Capability<Session>);
	GENODE_RPC(Rpc_session_control, void, session_control, Label, Session_control);
	GENODE_RPC_THROW(Rpc_buffer, void, buffer, GENODE_TYPE_LIST(Out_of_metadata),
	                 Framebuffer::Mode, bool);

	GENODE_RPC_INTERFACE(Rpc_framebuffer_session, Rpc_input_session,
	                     Rpc_create_view, Rpc_destroy_view, Rpc_view_handle,
	                     Rpc_view_capability, Rpc_release_view_handle,
	                     Rpc_command_dataspace, Rpc_execute, Rpc_mode,
	                     Rpc_mode_sigh, Rpc_buffer, Rpc_focus, Rpc_session_control);
};

#endif /* _INCLUDE__NITPICKER_SESSION__NITPICKER_SESSION_H_ */
