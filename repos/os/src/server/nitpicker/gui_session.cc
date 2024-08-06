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


bool Gui_session::_views_are_equal(View_handle v1, View_handle v2)
{
	if (!v1.valid() || !v2.valid())
		return false;

	Weak_ptr<View> v1_ptr = _view_handle_registry.lookup(v1);
	Weak_ptr<View> v2_ptr = _view_handle_registry.lookup(v2);

	return  v1_ptr == v2_ptr;
}


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
	switch (command.opcode) {

	case Command::GEOMETRY:
		{
			Command::Geometry const &cmd = command.geometry;
			Locked_ptr<View> view(_view_handle_registry.lookup(cmd.view));
			if (!view.valid())
				return;

			Point pos = cmd.rect.p1();

			/* transpose position of top-level views by vertical session offset */
			if (view->top_level())
				pos = _phys_pos(pos, _view_stack.size());

			if (view.valid())
				_view_stack.geometry(*view, Rect(pos, cmd.rect.area));

			return;
		}

	case Command::OFFSET:
		{
			Command::Offset const &cmd = command.offset;
			Locked_ptr<View> view(_view_handle_registry.lookup(cmd.view));

			if (view.valid())
				_view_stack.buffer_offset(*view, cmd.offset);

			return;
		}

	case Command::FRONT:
		{
			Command::Front const &cmd = command.front;

			Locked_ptr<View> view(_view_handle_registry.lookup(cmd.view));
			if (view.valid())
				_view_stack.stack(*view, nullptr, true);
			return;
		}

	case Command::BACK:
		{
			Command::Back const &cmd = command.back;

			Locked_ptr<View> view(_view_handle_registry.lookup(cmd.view));
			if (view.valid())
				_view_stack.stack(*view, nullptr, false);
			return;
		}

	case Command::FRONT_OF:
		{
			Command::Front_of const &cmd = command.front_of;
			if (_views_are_equal(cmd.view, cmd.neighbor))
				return;

			Locked_ptr<View> view(_view_handle_registry.lookup(cmd.view));
			if (!view.valid())
				return;

			/* stack view relative to neighbor */
			Locked_ptr<View> neighbor(_view_handle_registry.lookup(cmd.neighbor));
			if (neighbor.valid())
				_view_stack.stack(*view, &(*neighbor), false);
			return;
		}

	case Command::BEHIND_OF:
		{
			Command::Behind_of const &cmd = command.behind_of;
			if (_views_are_equal(cmd.view, cmd.neighbor))
				return;

			Locked_ptr<View> view(_view_handle_registry.lookup(cmd.view));
			if (!view.valid())
				return;

			/* stack view relative to neighbor */
			Locked_ptr<View> neighbor(_view_handle_registry.lookup(cmd.neighbor));
			if (neighbor.valid())
				_view_stack.stack(*view, &(*neighbor), true);
			return;
		}

	case Command::BACKGROUND:
		{
			Command::Background const &cmd = command.background;
			if (_provides_default_bg) {
				Locked_ptr<View> view(_view_handle_registry.lookup(cmd.view));
				if (!view.valid())
					return;

				view->background(true);
				_view_stack.default_background(*view);
				return;
			}

			/* revert old background view to normal mode */
			if (_background)
				_background->background(false);

			/* assign session background */
			Locked_ptr<View> view(_view_handle_registry.lookup(cmd.view));
			if (!view.valid())
				return;

			_background = &(*view);

			/* switch background view to background mode */
			if (background())
				view->background(true);

			return;
		}

	case Command::TITLE:
		{
			Command::Title const &cmd = command.title;
			Locked_ptr<View> view(_view_handle_registry.lookup(cmd.view));

			if (view.valid())
				_view_stack.title(*view, cmd.title.string());

			return;
		}

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


Gui_session::Create_view_result Gui_session::create_view()
{
	try {
		View &view = *new (_view_alloc)
			View(*this, _texture, { .transparent = false, .background = false }, nullptr);

		_adopt_new_view(view);

		return _view_handle_registry.alloc(view);
	}
	catch (Out_of_ram)  { return Create_view_error::OUT_OF_RAM; }
	catch (Out_of_caps) { return Create_view_error::OUT_OF_CAPS; }
}


Gui_session::Create_child_view_result Gui_session::create_child_view(View_handle const parent_handle)
{
	View *parent_view_ptr = nullptr;

	try {
		Locked_ptr<View> parent(_view_handle_registry.lookup(parent_handle));
		if (parent.valid())
			parent_view_ptr = &(*parent);
	}
	catch (View_handle_registry::Lookup_failed) { }

	if (!parent_view_ptr)
		return Create_child_view_error::INVALID;

	try {
		View &view = *new (_view_alloc)
			View(*this, _texture, { .transparent = false, .background = false }, parent_view_ptr);

		parent_view_ptr->add_child(view);

		_adopt_new_view(view);

		return _view_handle_registry.alloc(view);
	}
	catch (Out_of_ram)  { return Create_child_view_error::OUT_OF_RAM; }
	catch (Out_of_caps) { return Create_child_view_error::OUT_OF_CAPS; }
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


void Gui_session::destroy_view(View_handle handle)
{
	/*
	 * Search view object given the handle
	 *
	 * We cannot look up the view directly from the
	 * '_view_handle_registry' because we would obtain a weak
	 * pointer to the view object. If we called the object's
	 * destructor from the corresponding locked pointer, the
	 * call of 'lock_for_destruction' in the view's destructor
	 * would attempt to take the lock again.
	 */
	for (Session_view_list_elem *v = _view_list.first(); v; v = v->next()) {

		auto handle_matches = [&] (View const &view)
		{
			try { return _view_handle_registry.has_handle(view, handle); }

			/* 'Handle_registry::has_handle' may throw */
			catch (...) { return false; };
		};

		View &view = *static_cast<View *>(v);

		if (handle_matches(view)) {
			_destroy_view(view);
			_view_handle_registry.free(handle);
			break;
		}
	}

	_hover_updater.update_hover();
}


Gui_session::View_handle_result
Gui_session::view_handle(View_capability view_cap, View_handle handle)
{
	try {
		return _env.ep().rpc_ep().apply(view_cap, [&] (View *view) {
			return (view) ? _view_handle_registry.alloc(*view, handle)
			              : View_handle(); });
	}
	catch (View_handle_registry::Out_of_memory)  { return View_handle_error::OUT_OF_RAM; }
	catch (Out_of_ram)                           { return View_handle_error::OUT_OF_RAM; }
	catch (Out_of_caps)                          { return View_handle_error::OUT_OF_RAM; }
}


View_capability Gui_session::view_capability(View_handle handle)
{
	try {
		Locked_ptr<View> view(_view_handle_registry.lookup(handle));
		return view.valid() ? view->cap() : View_capability();
	}
	catch (View_handle_registry::Lookup_failed) { return View_capability(); }
}


void Gui_session::release_view_handle(View_handle handle)
{
	try {
		_view_handle_registry.free(handle); }

	catch (View_handle_registry::Lookup_failed) {
		warning("view lookup failed while releasing view handle");
		return;
	}
}


void Gui_session::execute()
{
	for (unsigned i = 0; i < _command_buffer.num(); i++) {
		try {
			_execute_command(_command_buffer.get(i)); }
		catch (View_handle_registry::Lookup_failed) {
			warning("view lookup failed during command execution"); }
	}
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
