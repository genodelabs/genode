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
#include <input_session/client.h>
#include <input/event.h>
#include <input/component.h>

/* local includes */
#include <types.h>
#include <window_registry.h>
#include <pointer.h>
#include <real_gui.h>

namespace Wm { class Main;
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


struct Wm::Decorator_gui_session : Genode::Session_object<Gui::Session>,
                                   private List<Decorator_gui_session>::Element,
                                   private Upgradeable
{
	friend class List<Decorator_gui_session>;
	using List<Decorator_gui_session>::Element::next;

	using View_capability = Gui::View_capability;
	using View_id         = Gui::View_id;
	using Command_buffer  = Gui::Session::Command_buffer;

	struct Content_view_ref : Gui::View_ref
	{
		Gui::View_ids::Element id;

		Window_registry::Id win_id;

		Content_view_ref(Window_registry::Id win_id, Gui::View_ids &ids, View_id id)
		: id(*this, ids, id), win_id(win_id) { }
	};

	Gui::View_ids _content_view_ids { };

	Genode::Env &_env;

	Genode::Constrained_ram_allocator _ram { _env.ram(), _ram_quota_guard(), _cap_quota_guard() };

	Genode::Sliced_heap _session_alloc { _ram, _env.rm() };

	Slab<Content_view_ref, 4000> _content_view_ref_alloc { _session_alloc };

	Real_gui _real_gui { _env, "decorator" };

	Input::Session_client _input_session { _env.rm(), _real_gui.session.input() };

	Genode::Signal_context_capability _mode_sigh { };

	Attached_ram_dataspace _client_command_ds { _ram, _env.rm(), sizeof(Command_buffer) };

	Command_buffer &_client_command_buffer = *_client_command_ds.local_addr<Command_buffer>();

	Pointer::State _pointer_state;

	Input::Session_component &_window_layouter_input;

	Decorator_content_callback &_content_callback;

	/* Gui::Connection requires a valid input session */
	Input::Session_component  _dummy_input_component { _env, _env.ram() };
	Input::Session_capability _dummy_input_component_cap =
		_env.ep().manage(_dummy_input_component);

	Signal_handler<Decorator_gui_session>
		_input_handler { _env.ep(), *this, &Decorator_gui_session::_handle_input };

	Window_registry::Id _win_id_from_title(Gui::Title const &title)
	{
		unsigned value = 0;
		Genode::ascii_to(title.string(), value);
		return { value };
	}

	Decorator_gui_session(Genode::Env                &env,
	                      Resources            const &resources,
	                      Label                const &label,
	                      Diag                 const &diag,
	                      Pointer::Tracker           &pointer_tracker,
	                      Input::Session_component   &window_layouter_input,
	                      Decorator_content_callback &content_callback)
	:
		Session_object<Gui::Session>(env.ep(), resources, label, diag),
		_env(env),
		_pointer_state(pointer_tracker),
		_window_layouter_input(window_layouter_input),
		_content_callback(content_callback)
	{
		_input_session.sigh(_input_handler);
	}

	~Decorator_gui_session()
	{
		_env.ep().dissolve(_dummy_input_component);
	}

	void upgrade_local_or_remote(Resources const &resources)
	{
		_upgrade_local_or_remote(resources, *this, _real_gui);
	}

	void _handle_input()
	{
		while (_input_session.pending())
			_input_session.for_each_event([&] (Input::Event const &ev) {
				_pointer_state.apply_event(ev);
				_window_layouter_input.submit(ev); });
	}

	void _execute_command(Command const &cmd)
	{
		switch (cmd.opcode) {

		case Command::GEOMETRY:

			/*
			 * If the content view changes position, propagate the new position
			 * to the GUI service to properly transform absolute input
			 * coordinates.
			 */
			_content_view_ids.apply<Content_view_ref const>(cmd.geometry.view,
				[&] (Content_view_ref const &view_ref) {
					_content_callback.content_geometry(view_ref.win_id, cmd.geometry.rect); },
				[&] { });

			/* forward command */
			_real_gui.enqueue(cmd);
			return;

		case Command::OFFSET:

			/*
			 * If non-content views change their offset (if the lookup
			 * fails), propagate the event
			 */
			_content_view_ids.apply<Content_view_ref const>(cmd.geometry.view,
				[&] (Content_view_ref const &) { },
				[&] { _real_gui.enqueue(cmd); });
			return;

		case Command::FRONT:
		case Command::BACK:
		case Command::FRONT_OF:
		case Command::BEHIND_OF:

			_content_view_ids.apply<Content_view_ref const>(cmd.front.view,
				[&] (Content_view_ref const &view_ref) {
					_content_callback.update_content_child_views(view_ref.win_id); },
				[&] { });

			_real_gui.enqueue(cmd);
			return;

		case Command::TITLE:
		case Command::BACKGROUND:
		case Command::NOP:

			_real_gui.enqueue(cmd);
			return;
		}
	}

	Pointer::Position last_observed_pointer_pos() const
	{
		return _pointer_state.last_observed_pos();
	}


	/***************************
	 ** GUI session interface **
	 ***************************/
	
	Framebuffer::Session_capability framebuffer() override
	{
		return _real_gui.session.framebuffer();
	}

	Input::Session_capability input() override
	{
		/*
		 * Deny input to the decorator. User input referring to the
		 * window decorations is routed to the window manager.
		 */
		return _dummy_input_component_cap;
	}

	View_result view(View_id id, View_attr const &attr) override
	{
		/*
		 * The decorator marks a content view by specifying the window ID
		 * as view title. For such views, we import the view from the
		 * corresponding GUI cient instead of creating a new view.
		 */
		Window_registry::Id const win_id = _win_id_from_title(attr.title);
		if (win_id.valid()) {
			try {
				Content_view_ref &view_ref_ptr = *new (_content_view_ref_alloc)
					Content_view_ref(Window_registry::Id(win_id), _content_view_ids, id);

				View_capability view_cap = _content_callback.content_view(win_id);
				Associate_result result = _real_gui.session.associate(id, view_cap);
				if (result != Associate_result::OK)
					destroy(_content_view_ref_alloc, &view_ref_ptr);

				switch (result) {
				case Associate_result::OUT_OF_RAM:  return View_result::OUT_OF_RAM;
				case Associate_result::OUT_OF_CAPS: return View_result::OUT_OF_CAPS;
				case Associate_result::OK:          return View_result::OK;
				case Associate_result::INVALID:     break; /* fall back to regular view */
				};
			}
			catch (Genode::Out_of_ram)  {
				_starved_for_ram = true;
				return View_result::OUT_OF_RAM;
			}
			catch (Genode::Out_of_caps) {
				_starved_for_caps = true;
				return View_result::OUT_OF_CAPS;
			}
		}
		return _real_gui.session.view(id, attr);
	}

	Child_view_result child_view(View_id id, View_id parent, View_attr const &attr) override
	{
		return _real_gui.session.child_view(id, parent, attr);
	}

	void destroy_view(View_id view) override
	{
		/*
		 * Reset view geometry when destroying a content view
		 */
		_content_view_ids.apply<Content_view_ref>(view,
			[&] (Content_view_ref &view_ref) {

				_content_callback.hide_content_child_views(view_ref.win_id);

				Gui::Rect rect(Gui::Point(0, 0), Gui::Area(0, 0));
				_real_gui.enqueue<Gui::Session::Command::Geometry>(view, rect);
				_real_gui.execute();

				destroy(_content_view_ref_alloc, &view_ref);
			},
			[&] { });

		_real_gui.session.destroy_view(view);
	}

	Associate_result associate(View_id id, View_capability view_cap) override
	{
		return _real_gui.session.associate(id, view_cap);
	}

	View_capability_result view_capability(View_id view) override
	{
		return _real_gui.session.view_capability(view);
	}

	void release_view_id(View_id view) override
	{
		_content_view_ids.apply<Content_view_ref>(view,
			[&] (Content_view_ref &view_ref) { destroy(_content_view_ref_alloc, &view_ref); },
			[&] { });

		_real_gui.session.release_view_id(view);
	}

	Genode::Dataspace_capability command_dataspace() override
	{
		return _client_command_ds.cap();
	}

	void execute() override
	{
		for (unsigned i = 0; i < _client_command_buffer.num(); i++)
			_execute_command(_client_command_buffer.get(i));

		_real_gui.execute();
	}

	Framebuffer::Mode mode() override
	{
		return _real_gui.session.mode();
	}

	void mode_sigh(Genode::Signal_context_capability sigh) override
	{
		/*
		 * Remember signal-context capability to keep NOVA from revoking
		 * transitive delegations of the capability.
		 */
		_mode_sigh = sigh;
		_real_gui.session.mode_sigh(sigh);
	}

	Buffer_result buffer(Framebuffer::Mode mode, bool use_alpha) override
	{
		return _real_gui.session.buffer(mode, use_alpha);
	}

	void focus(Genode::Capability<Gui::Session>) override { }
};

#endif /* _DECORATOR_GUI_H_ */
