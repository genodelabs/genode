/*
 * \brief  Nitpicker service provided to decorator
 * \author Norman Feske
 * \date   2014-02-14
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DECORATOR_NITPICKER_H_
#define _DECORATOR_NITPICKER_H_

/* Genode includes */
#include <util/string.h>
#include <ram_session/client.h>
#include <base/attached_dataspace.h>
#include <os/reporter.h>
#include <nitpicker_session/connection.h>
#include <input_session/client.h>
#include <input/event.h>
#include <input/component.h>

/* local includes */
#include <window_registry.h>
#include <last_motion.h>

namespace Wm { class Main;
	using Genode::size_t;
	using Genode::Allocator;
	using Genode::Ram_session_client;
	using Genode::Ram_session_capability;
	using Genode::Arg_string;
	using Genode::Object_pool;
	using Genode::Attached_dataspace;
	using Genode::Attached_ram_dataspace;
	using Genode::Signal_handler;
	using Genode::Reporter;
}


namespace Wm {

	struct Decorator_nitpicker_session;
	struct Decorator_content_callback;
	struct Decorator_content_registry;
}


struct Wm::Decorator_content_callback
{
	virtual void content_geometry(Window_registry::Id win_id, Rect rect) = 0;

	virtual Nitpicker::View_capability content_view(Window_registry::Id win_id) = 0;

	virtual void update_content_child_views(Window_registry::Id win_id) = 0;
};


class Wm::Decorator_content_registry
{
	public:

		/**
		 * Exception type
		 */
		struct Lookup_failed { };

	private:

		struct Entry : List<Entry>::Element
		{
			Nitpicker::Session::View_handle const decorator_view_handle;
			Window_registry::Id             const win_id;

			Entry(Nitpicker::Session::View_handle decorator_view_handle,
			      Window_registry::Id             win_id)
			:
				decorator_view_handle(decorator_view_handle),
				win_id(win_id)
			{ }
		};

		List<Entry>  _list;
		Allocator   &_entry_alloc;

		Entry const &_lookup(Nitpicker::Session::View_handle view_handle) const
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

		void insert(Nitpicker::Session::View_handle decorator_view_handle,
		            Window_registry::Id             win_id)
		{
			Entry *e = new (_entry_alloc) Entry(decorator_view_handle, win_id);
			_list.insert(e);
		}

		/**
		 * Lookup window ID for a given decorator content view
		 *
		 * \throw Lookup_failed
		 */
		Window_registry::Id lookup(Nitpicker::Session::View_handle view_handle) const
		{
			return _lookup(view_handle).win_id;
		}

		bool registered(Nitpicker::Session::View_handle view_handle) const
		{
			try { lookup(view_handle); return true; } catch (...) { }
			return false;
		}

		/**
		 * Remove entry
		 *
		 * \throw Lookup_failed
		 */
		void remove(Nitpicker::Session::View_handle view_handle)
		{
			_remove(_lookup(view_handle));
		}
};


struct Wm::Decorator_nitpicker_session : Genode::Rpc_object<Nitpicker::Session>,
                                         List<Decorator_nitpicker_session>::Element
{
	typedef Nitpicker::View_capability      View_capability;
	typedef Nitpicker::Session::View_handle View_handle;

	Genode::Env &_env;

	Genode::Heap _heap { _env.ram(), _env.rm() };

	Ram_session_client _ram;

	Nitpicker::Connection _nitpicker_session { _env, "decorator" };

	Genode::Signal_context_capability _mode_sigh;

	typedef Nitpicker::Session::Command_buffer Command_buffer;

	Attached_ram_dataspace _command_ds { _ram, _env.rm(), sizeof(Command_buffer) };

	Command_buffer &_command_buffer = *_command_ds.local_addr<Command_buffer>();

	Reporter &_pointer_reporter;

	Last_motion &_last_motion;

	Input::Session_component &_window_layouter_input;

	Decorator_content_callback &_content_callback;

	/* XXX don't allocate content-registry entries from heap */
	Decorator_content_registry _content_registry { _heap };

	Allocator &_md_alloc;

	/* Nitpicker::Connection requires a valid input session */
	Input::Session_component  _dummy_input_component { _env, _env.ram() };
	Input::Session_capability _dummy_input_component_cap =
		_env.ep().manage(_dummy_input_component);

	Signal_handler<Decorator_nitpicker_session>
		_input_handler { _env.ep(), *this, &Decorator_nitpicker_session::_handle_input };

	/**
	 * Constructor
	 *
	 * \param ep  entrypoint used for dispatching signals
	 */
	Decorator_nitpicker_session(Genode::Env &env,
	                            Ram_session_capability ram,
	                            Allocator &md_alloc,
	                            Reporter &pointer_reporter,
	                            Last_motion &last_motion,
	                            Input::Session_component &window_layouter_input,
	                            Decorator_content_callback &content_callback)
	:
		_env(env),
		_ram(ram),
		_pointer_reporter(pointer_reporter),
		_last_motion(last_motion),
		_window_layouter_input(window_layouter_input),
		_content_callback(content_callback),
		_md_alloc(md_alloc)
	{
		_nitpicker_session.input()->sigh(_input_handler);
	}

	void _handle_input()
	{
		while (_nitpicker_session.input()->pending())
			_nitpicker_session.input()->for_each_event([&] (Input::Event const &ev) {

				if (ev.type() == Input::Event::MOTION) {

					_last_motion = LAST_MOTION_DECORATOR;

					Reporter::Xml_generator xml(_pointer_reporter, [&] ()
					{
						xml.attribute("xpos", ev.ax());
						xml.attribute("ypos", ev.ay());
					});
				}

				if (ev.type() == Input::Event::LEAVE) {

					/*
					 * Invalidate pointer as reported to the decorator if the
					 * pointer moved from a window decoration to a position
					 * with no window known to the window manager. If the last
					 * motion referred to one of the regular client session,
					 * this is not needed because the respective session will
					 * update the pointer model with the entered position
					 * already.
					 */
					if (_last_motion == LAST_MOTION_DECORATOR) {
						Reporter::Xml_generator xml(_pointer_reporter, [&] ()
						{ });
					}
				}

				_window_layouter_input.submit(ev);
			});
	}

	void _execute_command(Command const &cmd)
	{
		switch (cmd.opcode) {

		case Command::OP_TITLE:
			{
				unsigned long id = 0;
				Genode::ascii_to(cmd.title.title.string(), id);

				if (id > 0)
					_content_registry.insert(cmd.title.view,
					                         Window_registry::Id(id));
				return;
			}

		case Command::OP_TO_FRONT:

			try {

				/*
				 * If the content view is re-stacked, replace it by the real
				 * window content.
				 *
				 * The lookup fails with an exception for non-content views.
				 * In this case, forward the command.
				 */
				Window_registry::Id win_id = _content_registry.lookup(cmd.to_front.view);

				/*
				 * Replace content view originally created by the decorator
				 * by view that shows the real window content.
				 */
				Nitpicker::View_capability view_cap =
					_content_callback.content_view(win_id);

				_nitpicker_session.view_handle(view_cap,
				                               cmd.to_front.view);

				_nitpicker_session.enqueue(cmd);
				_nitpicker_session.execute();

				/*
				 * Now that the physical content view exists, it is time
				 * to revisit the child views.
				 */
				_content_callback.update_content_child_views(win_id);

			} catch (Decorator_content_registry::Lookup_failed) {

				_nitpicker_session.enqueue(cmd);
			}

			return;

		case Command::OP_GEOMETRY:

			try {

				/*
				 * If the content view changes position, propagate the new
				 * position to the nitpicker service to properly transform
				 * absolute input coordinates.
				 */
				Window_registry::Id win_id = _content_registry.lookup(cmd.geometry.view);

				_content_callback.content_geometry(win_id, cmd.geometry.rect);
			}
			catch (Decorator_content_registry::Lookup_failed) { }

			/* forward command */
			_nitpicker_session.enqueue(cmd);
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
				_nitpicker_session.enqueue(cmd);
			}
			return;

		case Command::OP_TO_BACK:
		case Command::OP_BACKGROUND:
		case Command::OP_NOP:

			_nitpicker_session.enqueue(cmd);
			return;
		}
	}

	void upgrade(const char *args)
	{
		size_t const ram_quota = Arg_string::find_arg(args, "ram_quota").ulong_value(0);
		_nitpicker_session.upgrade_ram(ram_quota);
	}


	/*********************************
	 ** Nitpicker session interface **
	 *********************************/
	
	Framebuffer::Session_capability framebuffer_session() override
	{
		return _nitpicker_session.framebuffer_session();
	}

	Input::Session_capability input_session() override
	{
		/*
		 * Deny input to the decorator. User input referring to the
		 * window decorations is routed to the window manager.
		 */
		return _dummy_input_component_cap;
	}

	View_handle create_view(View_handle parent) override
	{
		return _nitpicker_session.create_view(parent);
	}

	void destroy_view(View_handle view) override
	{
		/*
		 * Reset view geometry when destroying a content view
		 */
		if (_content_registry.registered(view)) {
			Nitpicker::Rect rect(Nitpicker::Point(0, 0), Nitpicker::Area(0, 0));
			_nitpicker_session.enqueue<Nitpicker::Session::Command::Geometry>(view, rect);
			_nitpicker_session.execute();
		}

		_nitpicker_session.destroy_view(view);
	}

	View_handle view_handle(View_capability view_cap, View_handle handle) override
	{
		return _nitpicker_session.view_handle(view_cap, handle);
	}

	View_capability view_capability(View_handle view) override
	{
		return _nitpicker_session.view_capability(view);
	}

	void release_view_handle(View_handle view) override
	{
		/* XXX dealloc View_ptr */
		_nitpicker_session.release_view_handle(view);
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
		_nitpicker_session.execute();
	}

	Framebuffer::Mode mode() override
	{
		return _nitpicker_session.mode();
	}

	void mode_sigh(Genode::Signal_context_capability sigh) override
	{
		/*
		 * Remember signal-context capability to keep NOVA from revoking
		 * transitive delegations of the capability.
		 */
		_mode_sigh = sigh;
		_nitpicker_session.mode_sigh(sigh);
	}

	void buffer(Framebuffer::Mode mode, bool use_alpha) override
	{
		/*
		 * See comment in 'Wm::Nitpicker::Session_component::buffer'.
		 */
		Nitpicker::Session_client(_env.rm(), _nitpicker_session.cap()).buffer(mode, use_alpha);
	}

	void focus(Genode::Capability<Nitpicker::Session>) { }
};

#endif /* _DECORATOR_NITPICKER_H_ */
