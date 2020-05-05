/*
 * \brief  Nitpicker session component
 * \author Norman Feske
 * \date   2017-11-16
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include "view_stack.h"

using namespace Nitpicker;


void Session_component::_release_buffer()
{
	if (!_texture)
		return;

	typedef Pixel_rgb565 PT;

	Chunky_texture<PT> const *cdt = static_cast<Chunky_texture<PT> const *>(_texture);

	_texture    = nullptr;
	_uses_alpha = false;
	_input_mask = nullptr;

	destroy(&_session_alloc, const_cast<Chunky_texture<PT> *>(cdt));

	replenish(Ram_quota{_buffer_size});
	_buffer_size = 0;
}


bool Session_component::_views_are_equal(View_handle v1, View_handle v2)
{
	if (!v1.valid() || !v2.valid())
		return false;

	Weak_ptr<View_component> v1_ptr = _view_handle_registry.lookup(v1);
	Weak_ptr<View_component> v2_ptr = _view_handle_registry.lookup(v2);

	return  v1_ptr == v2_ptr;
}


View_owner &Session_component::forwarded_focus()
{
	Session_component *next_focus = this;

	/* helper used for detecting cycles */
	Session_component *next_focus_slow = next_focus;

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


void Session_component::_execute_command(Command const &command)
{
	switch (command.opcode) {

	case Command::OP_GEOMETRY:
		{
			Command::Geometry const &cmd = command.geometry;
			Locked_ptr<View_component> view(_view_handle_registry.lookup(cmd.view));
			if (!view.valid())
				return;

			Point pos = cmd.rect.p1();

			/* transpose position of top-level views by vertical session offset */
			if (view->top_level())
				pos = _phys_pos(pos, _view_stack.size());

			if (view.valid())
				_view_stack.geometry(*view, Rect(pos, cmd.rect.area()));

			return;
		}

	case Command::OP_OFFSET:
		{
			Command::Offset const &cmd = command.offset;
			Locked_ptr<View_component> view(_view_handle_registry.lookup(cmd.view));

			if (view.valid())
				_view_stack.buffer_offset(*view, cmd.offset);

			return;
		}

	case Command::OP_TO_FRONT:
		{
			Command::To_front const &cmd = command.to_front;
			if (_views_are_equal(cmd.view, cmd.neighbor))
				return;

			Locked_ptr<View_component> view(_view_handle_registry.lookup(cmd.view));
			if (!view.valid())
				return;

			/* bring to front if no neighbor is specified */
			if (!cmd.neighbor.valid()) {
				_view_stack.stack(*view, nullptr, true);
				return;
			}

			/* stack view relative to neighbor */
			Locked_ptr<View_component> neighbor(_view_handle_registry.lookup(cmd.neighbor));
			if (neighbor.valid())
				_view_stack.stack(*view, &(*neighbor), false);

			return;
		}

	case Command::OP_TO_BACK:
		{
			Command::To_back const &cmd = command.to_back;
			if (_views_are_equal(cmd.view, cmd.neighbor))
				return;

			Locked_ptr<View_component> view(_view_handle_registry.lookup(cmd.view));
			if (!view.valid())
				return;

			/* bring to front if no neighbor is specified */
			if (!cmd.neighbor.valid()) {
				_view_stack.stack(*view, nullptr, false);
				return;
			}

			/* stack view relative to neighbor */
			Locked_ptr<View_component> neighbor(_view_handle_registry.lookup(cmd.neighbor));
			if (neighbor.valid())
				_view_stack.stack(*view, &(*neighbor), true);

			return;
		}

	case Command::OP_BACKGROUND:
		{
			Command::Background const &cmd = command.background;
			if (_provides_default_bg) {
				Locked_ptr<View_component> view(_view_handle_registry.lookup(cmd.view));
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
			Locked_ptr<View_component> view(_view_handle_registry.lookup(cmd.view));
			if (!view.valid())
				return;

			_background = &(*view);

			/* switch background view to background mode */
			if (background())
				view->background(true);

			return;
		}

	case Command::OP_TITLE:
		{
			Command::Title const &cmd = command.title;
			Locked_ptr<View_component> view(_view_handle_registry.lookup(cmd.view));

			if (view.valid())
				_view_stack.title(*view, _font, cmd.title.string());

			return;
		}

	case Command::OP_NOP:
		return;
	}
}


void Session_component::_destroy_view(View_component &view)
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


void Session_component::destroy_all_views()
{
	while (Session_view_list_elem *v = _view_list.first())
		_destroy_view(*static_cast<View_component *>(v));
}


void Session_component::submit_input_event(Input::Event e)
{
	using namespace Input;

	Point const origin_offset = _phys_pos(Point(0, 0), _view_stack.size());

	/*
	 * Transpose absolute coordinates by session-specific vertical offset.
	 */
	e.handle_absolute_motion([&] (int x, int y) {
		e = Absolute_motion{max(0, x - origin_offset.x()),
		                    max(0, y - origin_offset.y())}; });
	e.handle_touch([&] (Touch_id id, float x, float y) {
		e = Touch{ id, max(0.0f, x - origin_offset.x()),
		               max(0.0f, y - origin_offset.y())}; });

	_input_session_component.submit(&e);
}


Session_component::View_handle Session_component::create_view(View_handle parent_handle)
{
	View_component *view = nullptr;

	/*
	 * Create child view
	 */
	if (parent_handle.valid()) {

		try {
			Locked_ptr<View_component> parent(_view_handle_registry.lookup(parent_handle));
			if (!parent.valid())
				return View_handle();

			view = new (_view_alloc)
				View_component(*this,
				               View_component::NOT_TRANSPARENT, View_component::NOT_BACKGROUND,
				               &(*parent));

			parent->add_child(*view);
		}
		catch (View_handle_registry::Lookup_failed) { return View_handle(); }
		catch (View_handle_registry::Out_of_memory) { throw Out_of_ram(); }
	}

	/*
	 * Create top-level view
	 */
	else {
		try {
			view = new (_view_alloc)
				View_component(*this,
				               View_component::NOT_TRANSPARENT, View_component::NOT_BACKGROUND,
				               nullptr);
		}
		catch (Allocator::Out_of_memory) { throw Out_of_ram(); }
	}

	view->title(_font, "");
	view->apply_origin_policy(_pointer_origin);

	_view_list.insert(view);
	_env.ep().manage(*view);

	try {
		return _view_handle_registry.alloc(*view);
	}
	catch (View_handle_registry::Out_of_memory) { throw Out_of_ram(); }
}


void Session_component::apply_session_policy(Xml_node config,
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

		typedef Domain_registry::Entry::Name Name;

		Name const name = policy.attribute_value("domain", Name());

		_domain = domain_registry.lookup(name);

		if (!_domain)
			error("policy for label \"", _label,
			      "\" specifies nonexistent domain \"", name, "\"");

	} catch (...) {
		error("no policy matching label \"", _label, "\""); }
}


void Session_component::destroy_view(View_handle handle)
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

		auto handle_matches = [&] (View_component const &view)
		{
			try { return _view_handle_registry.has_handle(view, handle); }

			/* 'Handle_registry::has_handle' may throw */
			catch (...) { return false; };
		};

		View_component &view = *static_cast<View_component *>(v);

		if (handle_matches(view)) {
			_destroy_view(view);
			_view_handle_registry.free(handle);
			break;
		}
	}
}


Session_component::View_handle
Session_component::view_handle(View_capability view_cap, View_handle handle)
{
	auto lambda = [&] (View_component *view)
	{
		return (view) ? _view_handle_registry.alloc(*view, handle)
		              : View_handle();
	};

	try {
		return _env.ep().rpc_ep().apply(view_cap, lambda);
	}
	catch (View_handle_registry::Out_of_memory) { throw Out_of_ram(); }
}


View_capability Session_component::view_capability(View_handle handle)
{
	try {
		Locked_ptr<View_component> view(_view_handle_registry.lookup(handle));
		return view.valid() ? view->cap() : View_capability();
	}
	catch (View_handle_registry::Lookup_failed) { return View_capability(); }
}


void Session_component::release_view_handle(View_handle handle)
{
	try {
		_view_handle_registry.free(handle); }

	catch (View_handle_registry::Lookup_failed) {
		warning("view lookup failed while releasing view handle");
		return;
	}
}


void Session_component::execute()
{
	for (unsigned i = 0; i < _command_buffer.num(); i++) {
		try {
			_execute_command(_command_buffer.get(i)); }
		catch (View_handle_registry::Lookup_failed) {
			warning("view lookup failed during command execution"); }
	}
}

Framebuffer::Mode Session_component::mode()
{
	Area const phys_area(_framebuffer.mode().width(),
	                     _framebuffer.mode().height());

	Area const session_area = screen_area(phys_area);

	return Framebuffer::Mode(session_area.w(), session_area.h(),
	                         _framebuffer.mode().format());
}


void Session_component::buffer(Framebuffer::Mode mode, bool use_alpha)
{
	/* check if the session quota suffices for the specified mode */
	if (_buffer_size + _ram_quota_guard().avail().value < ram_quota(mode, use_alpha))
		throw Out_of_ram();

	/* buffer re-allocation may consume new dataspace capability if buffer is new */
	if (_cap_quota_guard().avail().value < 1)
		throw Out_of_caps();

	_framebuffer_session_component.notify_mode_change(mode, use_alpha);
}


void Session_component::focus(Capability<Nitpicker::Session> session_cap)
{
	if (this->cap() == session_cap)
		return;

	_forwarded_focus = nullptr;

	_env.ep().rpc_ep().apply(session_cap, [&] (Session_component *session) {
		if (session)
			_forwarded_focus = session; });

	_focus_updater.update_focus();
}


void Session_component::session_control(Label suffix, Session_control control)
{
	switch (control) {
	case SESSION_CONTROL_HIDE:
		_visibility_controller.hide_matching_sessions(label(), suffix);
		break;

	case SESSION_CONTROL_SHOW:
		_visibility_controller.show_matching_sessions(label(), suffix);
		break;

	case SESSION_CONTROL_TO_FRONT:
		_view_stack.to_front(Label(label(), suffix).string());
		break;
	}
}


Buffer *Session_component::realloc_buffer(Framebuffer::Mode mode, bool use_alpha)
{
	typedef Pixel_rgb565 PT;

	Area const size(mode.width(), mode.height());

	_buffer_size = Chunky_texture<PT>::calc_num_bytes(size, use_alpha);

	/*
	 * Preserve the content of the original buffer if nitpicker has
	 * enough slack memory to temporarily keep the original pixels.
	 */
	Texture<PT> const *src_texture = nullptr;
	if (texture()) {

		enum { PRESERVED_RAM = 128*1024 };
		if (_env.pd().avail_ram().value > _buffer_size + PRESERVED_RAM) {
			src_texture = static_cast<Texture<PT> const *>(texture());
		} else {
			warning("not enough RAM to preserve buffer content during resize");
			_release_buffer();
		}
	}

	Ram_quota const temporary_ram_upgrade = src_texture
	                                      ? Ram_quota{_buffer_size} : Ram_quota{0};

	_ram_quota_guard().upgrade(temporary_ram_upgrade);

	auto try_alloc_texture = [&] ()
	{
		try {
			return new (&_session_alloc)
				Chunky_texture<PT>(_env.ram(), _env.rm(), size, use_alpha);
		} catch (...) {
			return (Chunky_texture<PT>*)nullptr;
		}
	};

	Chunky_texture<PT> * const texture = try_alloc_texture();

	/* copy old buffer content into new buffer and release old buffer */
	if (src_texture) {

		Surface<PT> surface(texture->pixel(),
		                    texture->Texture_base::size());

		Texture_painter::paint(surface, *src_texture, Color(), Point(0, 0),
		                       Texture_painter::SOLID, false);
		_release_buffer();

		if (!_ram_quota_guard().try_downgrade(temporary_ram_upgrade))
			warning("accounting error during framebuffer realloc");

	}

	try { withdraw(Ram_quota{_buffer_size}); }
	catch (...) {
		destroy(&_session_alloc, texture);
		_buffer_size = 0;
		return nullptr;
	}

	_texture    = texture;
	_uses_alpha = use_alpha;
	_input_mask = texture->input_mask_buffer();

	return texture;
}
