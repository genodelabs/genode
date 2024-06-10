/*
 * \brief  GUI service provided to decorator
 * \author Norman Feske
 * \date   2014-02-14
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DECORATOR_GUI_H_
#define _DECORATOR_GUI_H_

/* Genode includes */
#include <util/string.h>
#include <base/attached_dataspace.h>
#include <gui_session/connection.h>
#include <input_session/client.h>
#include <input/event.h>
#include <input/component.h>

/* local includes */
#include <window_registry.h>
#include <pointer.h>

namespace Wm { class Main;
	using Genode::size_t;
	using Genode::Allocator;
	using Genode::Arg_string;
	using Genode::Object_pool;
	using Genode::Attached_dataspace;
	using Genode::Attached_ram_dataspace;
	using Genode::Signal_handler;
	using Genode::Reporter;
	using Genode::Interface;
}


namespace Wm {

	struct Decorator_gui_session;
	struct Decorator_content_callback;
	struct Decorator_content_registry;
}


struct Wm::Decorator_content_callback : Interface
{
	virtual void content_geometry(Window_registry::Id win_id, Rect rect) = 0;

	virtual Gui::View_capability content_view(Window_registry::Id win_id) = 0;

	virtual void update_content_child_views(Window_registry::Id win_id) = 0;

	virtual void hide_content_child_views(Window_registry::Id win_id) = 0;
};


class Wm::Decorator_content_registry
{
	public:

		/**
		 * Exception type
		 */
		struct Lookup_failed { };

	private:

		using View_handle = Gui::Session::View_handle;

		struct Entry : List<Entry>::Element
		{
			View_handle         const decorator_view_handle;
			Window_registry::Id const win_id;

			Entry(View_handle decorator_view_handle, Window_registry::Id win_id)
			:
				decorator_view_handle(decorator_view_handle),
				win_id(win_id)
			{ }
		};

		List<Entry>  _list { };
		Allocator   &_entry_alloc;

		Entry const &_lookup(View_handle view_handle) const
		{
			for (Entry const *e = _list.first(); e; e = e->next()) {
				if (e->decorator_view_handle == view_handle)
					return *e;
			}

			throw Lookup_failed();
		}

		void _remove(Entry const &e)
		{
			_list.remove(&e);
			destroy(_entry_alloc, const_cast<Entry *>(&e));
		}

	public:

		Decorator_content_registry(Allocator &entry_alloc)
		:
			_entry_alloc(entry_alloc)
		{ }

		~Decorator_content_registry()
		{
			while (Entry *e = _list.first())
				_remove(*e);
		}

		void insert(View_handle decorator_view_handle, Window_registry::Id win_id)
		{
			Entry *e = new (_entry_alloc) Entry(decorator_view_handle, win_id);
			_list.insert(e);
		}

		/**
		 * Lookup window ID for a given decorator content view
		 *
		 * \throw Lookup_failed
		 */
		Window_registry::Id lookup(View_handle view_handle) const
		{
			return _lookup(view_handle).win_id;
		}

		bool registered(View_handle view_handle) const
		{
			try { lookup(view_handle); return true; } catch (...) { }
			return false;
		}

		/**
		 * Remove entry
		 *
		 * \throw Lookup_failed
		 */
		void remove(View_handle view_handle)
		{
			_remove(_lookup(view_handle));
		}
};


struct Wm::Decorator_gui_session : Genode::Rpc_object<Gui::Session>,
                                   private List<Decorator_gui_session>::Element
{
	friend class List<Decorator_gui_session>;
	using List<Decorator_gui_session>::Element::next;

	using View_capability = Gui::View_capability;
	using View_handle     = Gui::Session::View_handle;
	using Command_buffer  = Gui::Session::Command_buffer;

	Genode::Env &_env;

	Genode::Heap _heap { _env.ram(), _env.rm() };

	Genode::Ram_allocator &_ram;

	Gui::Connection _gui_session { _env, "decorator" };

	Genode::Signal_context_capability _mode_sigh { };

	Attached_ram_dataspace _command_ds { _ram, _env.rm(), sizeof(Command_buffer) };

	Command_buffer &_command_buffer = *_command_ds.local_addr<Command_buffer>();

	Pointer::State _pointer_state;

	Input::Session_component &_window_layouter_input;

	Decorator_content_callback &_content_callback;

	/* XXX don't allocate content-registry entries from heap */
	Decorator_content_registry _content_registry { _heap };

	Allocator &_md_alloc;

	/* Gui::Connection requires a valid input session */
	Input::Session_component  _dummy_input_component { _env, _env.ram() };
	Input::Session_capability _dummy_input_component_cap =
		_env.ep().manage(_dummy_input_component);

	Signal_handler<Decorator_gui_session>
		_input_handler { _env.ep(), *this, &Decorator_gui_session::_handle_input };

	/**
	 * Constructor
	 *
	 * \param ep  entrypoint used for dispatching signals
	 */
	Decorator_gui_session(Genode::Env &env,
	                      Genode::Ram_allocator &ram,
	                      Allocator &md_alloc,
	                      Pointer::Tracker &pointer_tracker,
	                      Input::Session_component &window_layouter_input,
	                      Decorator_content_callback &content_callback)
	:
		_env(env),
		_ram(ram),
		_pointer_state(pointer_tracker),
		_window_layouter_input(window_layouter_input),
		_content_callback(content_callback),
		_md_alloc(md_alloc)
	{
		_gui_session.input()->sigh(_input_handler);
	}

	~Decorator_gui_session()
	{
		_env.ep().dissolve(_dummy_input_component);
	}

	void _handle_input()
	{
		while (_gui_session.input()->pending())
			_gui_session.input()->for_each_event([&] (Input::Event const &ev) {
				_pointer_state.apply_event(ev);
				_window_layouter_input.submit(ev); });
	}

	void _execute_command(Command const &cmd)
	{
		switch (cmd.opcode) {

		case Command::OP_TITLE:
			{
				unsigned id = 0;
				Genode::ascii_to(cmd.title.title.string(), id);

				if (id > 0)
					_content_registry.insert(cmd.title.view,
					                         Window_registry::Id(id));
				return;
			}

		case Command::OP_TO_FRONT:
		case Command::OP_TO_BACK:

			try {

				View_handle const view_handle = (cmd.opcode == Command::OP_TO_FRONT)
				                              ?  cmd.to_front.view
				                              :  cmd.to_back.view;

				/*
				 * If the content view is re-stacked, replace it by the real
				 * window content.
				 *
				 * The lookup fails with an exception for non-content views.
				 * In this case, forward the command.
				 */
				Window_registry::Id win_id = _content_registry.lookup(view_handle);

				/*
				 * Replace content view originally created by the decorator
				 * by view that shows the real window content.
				 */
				View_capability view_cap = _content_callback.content_view(win_id);

				_gui_session.destroy_view(view_handle);
				_gui_session.view_handle(view_cap, view_handle);

				_gui_session.enqueue(cmd);
				_gui_session.execute();

				/*
				 * Now that the physical content view exists, it is time
				 * to revisit the child views.
				 */
				_content_callback.update_content_child_views(win_id);

			} catch (Decorator_content_registry::Lookup_failed) {

				_gui_session.enqueue(cmd);
			}

			return;

		case Command::OP_GEOMETRY:

			try {

				/*
				 * If the content view changes position, propagate the new
				 * position to the GUI service to properly transform absolute
				 * input coordinates.
				 */
				Window_registry::Id win_id = _content_registry.lookup(cmd.geometry.view);

				_content_callback.content_geometry(win_id, cmd.geometry.rect);
			}
			catch (Decorator_content_registry::Lookup_failed) { }

			/* forward command */
			_gui_session.enqueue(cmd);
			return;

		case Command::OP_OFFSET:

			try {

				/*
				 * If non-content views change their offset (if the lookup
				 * fails), propagate the event
				 */
				_content_registry.lookup(cmd.geometry.view);
			}
			catch (Decorator_content_registry::Lookup_failed) {
				_gui_session.enqueue(cmd);
			}
			return;

		case Command::OP_BACKGROUND:
		case Command::OP_NOP:

			_gui_session.enqueue(cmd);
			return;
		}
	}

	void upgrade(const char *args)
	{
		_gui_session.upgrade(Genode::session_resources_from_args(args));
	}

	Pointer::Position last_observed_pointer_pos() const
	{
		return _pointer_state.last_observed_pos();
	}


	/***************************
	 ** GUI session interface **
	 ***************************/
	
	Framebuffer::Session_capability framebuffer_session() override
	{
		return _gui_session.framebuffer_session();
	}

	Input::Session_capability input_session() override
	{
		/*
		 * Deny input to the decorator. User input referring to the
		 * window decorations is routed to the window manager.
		 */
		return _dummy_input_component_cap;
	}

	View_handle create_view() override
	{
		return _gui_session.create_view();
	}

	View_handle create_child_view(View_handle parent) override
	{
		return _gui_session.create_child_view(parent);
	}

	void destroy_view(View_handle view) override
	{
		/*
		 * Reset view geometry when destroying a content view
		 */
		if (_content_registry.registered(view)) {
			Gui::Rect rect(Gui::Point(0, 0), Gui::Area(0, 0));
			_gui_session.enqueue<Gui::Session::Command::Geometry>(view, rect);
			_gui_session.execute();

			Window_registry::Id win_id = _content_registry.lookup(view);
			_content_callback.hide_content_child_views(win_id);
		}

		_gui_session.destroy_view(view);
	}

	View_handle view_handle(View_capability view_cap, View_handle handle) override
	{
		return _gui_session.view_handle(view_cap, handle);
	}

	View_capability view_capability(View_handle view) override
	{
		return _gui_session.view_capability(view);
	}

	void release_view_handle(View_handle view) override
	{
		/* XXX dealloc View_ptr */
		_gui_session.release_view_handle(view);
	}

	Genode::Dataspace_capability command_dataspace() override
	{
		return _command_ds.cap();
	}

	void execute() override
	{
		for (unsigned i = 0; i < _command_buffer.num(); i++) {
			try {
				_execute_command(_command_buffer.get(i));
			}
			catch (...) {
				Genode::warning("unhandled exception while processing command from decorator");
			}
		}
		_gui_session.execute();
	}

	Framebuffer::Mode mode() override
	{
		return _gui_session.mode();
	}

	void mode_sigh(Genode::Signal_context_capability sigh) override
	{
		/*
		 * Remember signal-context capability to keep NOVA from revoking
		 * transitive delegations of the capability.
		 */
		_mode_sigh = sigh;
		_gui_session.mode_sigh(sigh);
	}

	void buffer(Framebuffer::Mode mode, bool use_alpha) override
	{
		/*
		 * See comment in 'Wm::Gui::Session_component::buffer'.
		 */
		Gui::Session_client(_env.rm(), _gui_session.cap()).buffer(mode, use_alpha);
	}

	void focus(Genode::Capability<Gui::Session>) override { }
};

#endif /* _DECORATOR_GUI_H_ */
