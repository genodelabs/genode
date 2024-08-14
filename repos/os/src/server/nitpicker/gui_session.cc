/*
 * \brief  GUI session component
 * \author Norman Feske
 * \date   2017-11-16
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <view_stack.h>

using namespace Nitpicker;


View_owner &Gui_session::forwarded_focus()
{
	Gui_session *next_focus = this;

	/* helper used for detecting cycles */
	Gui_session *next_focus_slow = next_focus;

	for (bool odd = false; ; odd = !odd) {

		/* we found the final focus once the forwarding stops */
		if (!next_focus->_forwarded_focus)
			break;

		next_focus = next_focus->_forwarded_focus;

		/* advance 'next_focus_slow' every odd iteration only */
		if (odd)
			next_focus_slow = next_focus_slow->_forwarded_focus;

		/* a cycle is detected if 'next_focus' laps 'next_focus_slow' */
		if (next_focus == next_focus_slow) {
			error("cyclic focus forwarding by ", next_focus->label());
			break;
		}
	}

	return *next_focus;
}


void Gui_session::_execute_command(Command const &command)
{
	auto with_this = [&] (auto const &args, auto const &fn)
	{
		Gui_session::_with_view(args.view,
			[&] (View &view) { fn(view, args); },
			[&] /* ignore operations on non-existing views */ { });
	};

	switch (command.opcode) {

	case Command::GEOMETRY:

		with_this(command.geometry, [&] (View &view, Command::Geometry const &args) {
			Point pos = args.rect.p1();

			/* transpose position of top-level views by vertical session offset */
			if (view.top_level())
				pos = _phys_pos(pos, _view_stack.size());

			_view_stack.geometry(view, Rect(pos, args.rect.area));
		});
		return;

	case Command::OFFSET:

		with_this(command.offset, [&] (View &view, Command::Offset const &args) {
			_view_stack.buffer_offset(view, args.offset); });
		return;

	case Command::FRONT:

		with_this(command.front, [&] (View &view, auto const &) {
			_view_stack.stack(view, nullptr, true); });
		return;

	case Command::BACK:

		with_this(command.back, [&] (View &view, auto const &) {
			_view_stack.stack(view, nullptr, false); });
		return;

	case Command::FRONT_OF:

		with_this(command.front_of, [&] (View &view, Command::Front_of const &args) {
			Gui_session::_with_view(args.neighbor,
				[&] (View &neighbor) {
					if (&view != &neighbor)
						_view_stack.stack(view, &neighbor, false); },
				[&] { });
		});
		return;

	case Command::BEHIND_OF:

		with_this(command.behind_of, [&] (View &view, Command::Behind_of const &args) {
			Gui_session::_with_view(args.neighbor,
				[&] (View &neighbor) {
					if (&view != &neighbor)
						_view_stack.stack(view, &neighbor, false); },
				[&] /* neighbor view does not exist */ { });
		});
		return;

	case Command::BACKGROUND:

		with_this(command.background, [&] (View &view, auto const &) {

			if (_provides_default_bg) {
				view.background(true);
				_view_stack.default_background(view);
				return;
			}

			/* revert old background view to normal mode */
			if (_background)
				_background->background(false);

			/* assign session background */
			_background = &view;

			/* switch background view to background mode */
			if (background())
				view.background(true);
		});
		return;

	case Command::TITLE:

		with_this(command.title, [&] (View &view, Command::Title const &args) {
			_view_stack.title(view, args.title); });
		return;

	case Command::NOP:
		return;
	}
}


void Gui_session::_destroy_view(View &view)
{
	if (_background == &view)
		_background = nullptr;

	/* reset background if view was used as default background */
	if (_view_stack.is_default_background(view))
		_view_stack.default_background(_builtin_background);

	_view_stack.remove_view(view);
	_env.ep().dissolve(view);
	_view_list.remove(&view);
	destroy(_view_alloc, &view);
}


void Gui_session::destroy_all_views()
{
	while (Session_view_list_elem *v = _view_list.first())
		_destroy_view(*static_cast<View *>(v));
}


void Gui_session::submit_input_event(Input::Event e)
{
	using namespace Input;

	Point const origin_offset = _phys_pos(Point(0, 0), _view_stack.size());

	/*
	 * Transpose absolute coordinates by session-specific vertical offset.
	 */
	e.handle_absolute_motion([&] (int x, int y) {
		e = Absolute_motion{max(0, x - origin_offset.x),
		                    max(0, y - origin_offset.y)}; });
	e.handle_touch([&] (Touch_id id, float x, float y) {
		e = Touch{ id, max(0.0f, x - (float)origin_offset.x),
		               max(0.0f, y - (float)origin_offset.y)}; });

	_input_session_component.submit(&e);
}


void Gui_session::_adopt_new_view(View &view)
{
	_view_stack.title(view, "");
	view.apply_origin_policy(_pointer_origin);

	_view_list.insert(&view);
	_env.ep().manage(view);
}


Gui_session::View_result
Gui_session::_create_view_and_ref(View_id const id, View_attr const &attr, auto const &create_fn)
{
	release_view_id(id);

	View_result error { }; /* assigned only in the error case */
	try {
		View &view = create_fn();
		try {
			new (_view_ref_alloc) View_ref(view.weak_ptr(), _view_ids, id);
			_adopt_new_view(view);

			/* apply initial view attributes */
			_execute_command(Command::Title    { id, attr.title });
			_execute_command(Command::Geometry { id, attr.rect  });
			if (attr.front)
				_execute_command(Command::Front { id });

			return View_result::OK;
		}
		catch (Out_of_ram)  { error = View_result::OUT_OF_RAM;  }
		catch (Out_of_caps) { error = View_result::OUT_OF_CAPS; }
		destroy(_view_alloc, &view);
	}
	catch (Out_of_ram)  { error = View_result::OUT_OF_RAM;  }
	catch (Out_of_caps) { error = View_result::OUT_OF_CAPS; }
	return error;
}


Gui_session::View_result Gui_session::view(View_id const id, View_attr const &attr)
{
	return _create_view_and_ref(id, attr, [&] () -> View & {
		return *new (_view_alloc)
			View(*this, _texture,
			     { .transparent = false, .background = false },
			     nullptr);
	});
}


Gui_session::Child_view_result
Gui_session::child_view(View_id const id, View_id const parent, View_attr const &attr)
{
	return _with_view(parent,
		[&] (View &parent) -> Child_view_result {

			View *view_ptr = nullptr;
			View_result const result = _create_view_and_ref(id, attr, [&] () -> View & {
				view_ptr = new (_view_alloc)
					View(*this, _texture,
					     { .transparent = false, .background = false },
					     &parent);
				return *view_ptr;
			});

			switch (result) {
			case View_result::OUT_OF_RAM:  return Child_view_result::OUT_OF_RAM;
			case View_result::OUT_OF_CAPS: return Child_view_result::OUT_OF_CAPS;
			case View_result::OK:          break;
			}
			if (view_ptr)
				parent.add_child(*view_ptr);
			return Child_view_result::OK;
		},
		[&] /* parent view does not exist */ () -> Child_view_result {
			return Child_view_result::INVALID; }
	);
}


void Gui_session::apply_session_policy(Xml_node config,
                                       Domain_registry const &domain_registry)
{
	reset_domain();

	try {
		Session_policy policy(_label, config);

		/* read domain attribute */
		if (!policy.has_attribute("domain")) {
			error("policy for label \"", _label, "\" lacks domain declaration");
			return;
		}

		using Name = Domain_registry::Entry::Name;

		Name const name = policy.attribute_value("domain", Name());

		_domain = domain_registry.lookup(name);

		if (!_domain)
			error("policy for label \"", _label,
			      "\" specifies nonexistent domain \"", name, "\"");

	} catch (...) {
		error("no policy matching label \"", _label, "\""); }
}


void Gui_session::destroy_view(View_id const id)
{
	/*
	 * Search among the session's own views the one with the given ID
	 */
	_with_view(id,
		[&] (View &view) {
			for (Session_view_list_elem *v = _view_list.first(); v; v = v->next())
				if (&view == v) {
					_destroy_view(view);
					break;
				}
		},
		[&] /* ID exists but view vanished */ { }
	);
	release_view_id(id);

	_hover_updater.update_hover();
}


Gui_session::Associate_result
Gui_session::associate(View_id id, View_capability view_cap)
{
	/* prevent ID conflict in 'View_ids::Element' constructor */
	release_view_id(id);

	return _env.ep().rpc_ep().apply(view_cap,
		[&] (View *view_ptr) -> Associate_result {
			if (!view_ptr)
				return Associate_result::INVALID;
			try {
				new (_view_ref_alloc) View_ref(view_ptr->weak_ptr(), _view_ids, id);
				return Associate_result::OK;
			}
			catch (Out_of_ram)  { return Associate_result::OUT_OF_RAM;  }
			catch (Out_of_caps) { return Associate_result::OUT_OF_CAPS; }
		});
}


View_capability Gui_session::view_capability(View_id id)
{
	return _with_view(id,
		[&] (View &view)               { return view.cap(); },
		[&] /* view does not exist */  { return View_capability(); });
}


void Gui_session::release_view_id(View_id id)
{
	_view_ids.apply<View_ref>(id,
		[&] (View_ref &view_ref) { destroy(_view_ref_alloc, &view_ref); },
		[&] { });
}


void Gui_session::execute()
{
	for (unsigned i = 0; i < _command_buffer.num(); i++)
		_execute_command(_command_buffer.get(i));

	_hover_updater.update_hover();
}


Framebuffer::Mode Gui_session::mode()
{
	Area const screen = screen_area(_view_stack.size());

	/*
	 * Return at least a size of 1x1 to spare the clients the need to handle
	 * the special case of 0x0, which can happen at boot time before the
	 * framebuffer driver is running.
	 */
	return { .area = { max(screen.w, 1u), max(screen.h, 1u) } };
}


Gui_session::Buffer_result Gui_session::buffer(Framebuffer::Mode mode, bool use_alpha)
{
	/* check if the session quota suffices for the specified mode */
	if (_buffer_size + _ram_quota_guard().avail().value < ram_quota(mode, use_alpha))
		return Buffer_result::OUT_OF_RAM;

	/* buffer re-allocation may consume new dataspace capability if buffer is new */
	if (_cap_quota_guard().avail().value < 1)
		throw Buffer_result::OUT_OF_CAPS;

	_framebuffer_session_component.notify_mode_change(mode, use_alpha);
	return Buffer_result::OK;
}


void Gui_session::focus(Capability<Gui::Session> session_cap)
{
	if (this->cap() == session_cap)
		return;

	_forwarded_focus = nullptr;

	_env.ep().rpc_ep().apply(session_cap, [&] (Gui_session *session) {
		if (session)
			_forwarded_focus = session; });

	_focus_updater.update_focus();
}


Dataspace_capability Gui_session::realloc_buffer(Framebuffer::Mode mode, bool use_alpha)
{
	Ram_quota const next_buffer_size { Chunky_texture<Pixel>::calc_num_bytes(mode.area, use_alpha) };
	Ram_quota const orig_buffer_size { _buffer_size };

	/*
	 * Preserve the content of the original buffer if nitpicker has
	 * enough slack memory to temporarily keep the original pixels.
	 */

	enum { PRESERVED_RAM = 128*1024 };
	bool const preserve_content =
		(_env.pd().avail_ram().value > next_buffer_size.value + PRESERVED_RAM);

	if (!preserve_content) {
		warning("not enough RAM to preserve buffer content during resize");
		_texture.release_current();
		replenish(orig_buffer_size);
	}

	_buffer_size = 0;
	_uses_alpha  = false;
	_input_mask  = nullptr;

	Ram_quota const temporary_ram_upgrade = _texture.valid()
	                                      ? next_buffer_size : Ram_quota{0};

	_ram_quota_guard().upgrade(temporary_ram_upgrade);

	if (!_texture.try_construct_next(_env.ram(), _env.rm(), mode.area, use_alpha)) {
		_texture.release_current();
		replenish(orig_buffer_size);
		_ram_quota_guard().try_downgrade(temporary_ram_upgrade);
		return Dataspace_capability();
	}

	_texture.switch_to_next();

	/* 'switch_to_next' has released the current texture */
	if (preserve_content)
		replenish(orig_buffer_size);

	if (!_ram_quota_guard().try_downgrade(temporary_ram_upgrade))
		warning("accounting error during framebuffer realloc");

	try { withdraw(next_buffer_size); }
	catch (...) {
		_texture.release_current();
		return Dataspace_capability();
	}

	_buffer_size = next_buffer_size.value;
	_uses_alpha  = use_alpha;
	_input_mask  = _texture.input_mask_buffer();

	return _texture.dataspace();
}
