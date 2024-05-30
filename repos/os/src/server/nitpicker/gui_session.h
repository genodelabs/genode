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

#ifndef _GUI_SESSION_H_
#define _GUI_SESSION_H_

/* Genode includes */
#include <util/list.h>
#include <base/session_object.h>
#include <base/heap.h>
#include <os/session_policy.h>
#include <os/reporter.h>
#include <os/pixel_rgb888.h>
#include <blit/painter.h>
#include <gui_session/gui_session.h>

/* local includes */
#include "canvas.h"
#include "domain_registry.h"
#include "framebuffer_session.h"
#include "input_session.h"
#include "focus.h"
#include "view.h"

namespace Nitpicker {

	class Gui_session;
	class View;

	typedef List<Gui_session> Session_list;
}


class Nitpicker::Gui_session : public  Session_object<Gui::Session>,
                               public  View_owner,
                               public  Buffer_provider,
                               private Session_list::Element
{
	private:

		friend class List<Gui_session>;

		using Gui::Session::Label;

		/*
		 * Noncopyable
		 */
		Gui_session(Gui_session const &);
		Gui_session &operator = (Gui_session const &);

		Env &_env;

		Constrained_ram_allocator _ram;

		Resizeable_texture<Pixel> _texture { };

		Domain_registry::Entry const *_domain     = nullptr;
		View                         *_background = nullptr;

		/*
		 * The input mask buffer containing a byte value per texture pixel,
		 * which describes the policy of handling user input referring to the
		 * pixel. If set to zero, the input is passed through the view such
		 * that it can be handled by one of the subsequent views in the view
		 * stack. If set to one, the input is consumed by the view. If
		 * 'input_mask' is a null pointer, user input is unconditionally
		 * consumed by the view.
		 */
		unsigned char const *_input_mask = nullptr;

		bool _uses_alpha = false;
		bool _visible    = true;

		Sliced_heap _session_alloc;

		Framebuffer::Session_component _framebuffer_session_component;

		bool const _input_session_accounted = (
			withdraw(Ram_quota{Input::Session_component::ev_ds_size()}), true );

		Input::Session_component _input_session_component { _env };

		View_stack &_view_stack;

		Focus_updater &_focus_updater;
		Hover_updater &_hover_updater;

		Signal_context_capability _mode_sigh { };

		View &_pointer_origin;

		View &_builtin_background;

		List<Session_view_list_elem> _view_list { };

		Tslab<View, 4000> _view_alloc { &_session_alloc };

		/* capabilities for sub sessions */
		Framebuffer::Session_capability _framebuffer_session_cap;
		Input::Session_capability       _input_session_cap;

		bool const _provides_default_bg;

		/* size of currently allocated virtual framebuffer, in bytes */
		size_t _buffer_size = 0;

		Attached_ram_dataspace _command_ds { _env.ram(), _env.rm(),
		                                     sizeof(Command_buffer) };

		bool const _command_buffer_accounted = (
			withdraw(Ram_quota{align_addr(sizeof(Session::Command_buffer), 12)}), true );

		Command_buffer &_command_buffer = *_command_ds.local_addr<Command_buffer>();

		typedef Handle_registry<View_handle, View> View_handle_registry;

		View_handle_registry _view_handle_registry;

		Reporter &_focus_reporter;

		Gui_session *_forwarded_focus = nullptr;

		/**
		 * Calculate session-local coordinate to physical screen position
		 *
		 * \param pos          coordinate in session-local coordinate system
		 * \param screen_area  session-local screen size
		 */
		Point _phys_pos(Point pos, Area screen_area) const
		{
			return _domain ? _domain->phys_pos(pos, screen_area) : Point(0, 0);
		}

		/**
		 * Helper for performing sanity checks in OP_TO_FRONT and OP_TO_BACK
		 *
		 * We have to check for the equality of both the specified view and
		 * neighbor. If both arguments refer to the same view, the creation of
		 * locked pointers for both views would result in a deadlock.
		 */
		bool _views_are_equal(View_handle, View_handle);

		void _execute_command(Command const &);

		void _destroy_view(View &);

	public:

		Gui_session(Env                   &env,
		            Resources       const &resources,
		            Label           const &label,
		            Diag            const &diag,
		            View_stack            &view_stack,
		            Focus_updater         &focus_updater,
		            Hover_updater         &hover_updater,
		            View                  &pointer_origin,
		            View                  &builtin_background,
		            bool                   provides_default_bg,
		            Reporter              &focus_reporter)
		:
			Session_object(env.ep(), resources, label, diag),
			_env(env),
			_ram(env.ram(), _ram_quota_guard(), _cap_quota_guard()),
			_session_alloc(_ram, env.rm()),
			_framebuffer_session_component(view_stack, *this, *this),
			_view_stack(view_stack),
			_focus_updater(focus_updater), _hover_updater(hover_updater),
			_pointer_origin(pointer_origin),
			_builtin_background(builtin_background),
			_framebuffer_session_cap(_env.ep().manage(_framebuffer_session_component)),
			_input_session_cap(_env.ep().manage(_input_session_component)),
			_provides_default_bg(provides_default_bg),
			_view_handle_registry(_session_alloc),
			_focus_reporter(focus_reporter)
		{ }

		~Gui_session()
		{
			_env.ep().dissolve(_framebuffer_session_component);
			_env.ep().dissolve(_input_session_component);

			destroy_all_views();
		}

		using Session_list::Element::next;


		/**************************
		 ** View_owner interface **
		 **************************/

		Session::Label label() const override { return _label; }

		/**
		 * Return true if session label starts with specified 'selector'
		 */
		bool matches_session_label(Session::Label const &selector) const override
		{
			/*
			 * Append label separator to match selectors with a trailing
			 * separator.
			 */
			String<Session_label::capacity() + 4> const label(_label, " ->");
			return strcmp(label.string(), selector.string(),
			              strlen(selector.string())) == 0;
		}

		bool visible() const override { return _visible; }

		bool label_visible() const override
		{
			return !_domain || _domain->label_visible();
		}

		bool has_same_domain(View_owner const *owner) const override
		{
			if (!owner) return false;
			return static_cast<Gui_session const &>(*owner)._domain == _domain;
		}

		bool has_focusable_domain() const override
		{
			return _domain && (_domain->focus_click() || _domain->focus_transient());
		}

		bool has_transient_focusable_domain() const override
		{
			return _domain && _domain->focus_transient();
		}

		Color color() const override { return _domain ? _domain->color() : white(); }

		bool content_client() const override { return _domain && _domain->content_client(); }

		bool hover_always() const override { return _domain && _domain->hover_always(); }

		View const *background() const override { return _background; }

		bool uses_alpha() const override { return _texture.valid() && _uses_alpha; }

		unsigned layer() const override { return _domain ? _domain->layer() : ~0U; }

		bool origin_pointer() const override { return _domain && _domain->origin_pointer(); }

		/**
		 * Return input mask value at specified buffer position
		 */
		unsigned char input_mask_at(Point p) const override
		{
			if (!_input_mask || !_texture.valid()) return 0;

			/* check boundaries */
			if ((unsigned)p.x() >= _texture.size().w()
			 || (unsigned)p.y() >= _texture.size().h())
				return 0;

			return _input_mask[p.y()*_texture.size().w() + p.x()];
		}

		void submit_input_event(Input::Event e) override;

		void report(Xml_generator &xml) const override
		{
			xml.attribute("label", _label);
			xml.attribute("color", String<32>(color()));

			if (_domain)
				xml.attribute("domain", _domain->name());
		}

		View_owner &forwarded_focus() override;


		/****************************************
		 ** Interface used by the main program **
		 ****************************************/

		/**
		 * Set the visibility of the views owned by the session
		 */
		void visible(bool visible) { _visible = visible; }

		/**
		 * Return session-local screen area
		 *
		 * \param phys_pos  size of physical screen
		 */
		Area screen_area(Area phys_area) const
		{
			return _domain ? _domain->screen_area(phys_area) : Area(0, 0);
		}

		void reset_domain() { _domain = nullptr; }

		/**
		 * Set session domain according to the list of configured policies
		 *
		 * Select the policy that matches the label. If multiple policies
		 * match, select the one with the largest number of characters.
		 */
		void apply_session_policy(Xml_node config, Domain_registry const &);

		void destroy_all_views();

		/**
		 * Deliver mode-change signal to client
		 */
		void notify_mode_change()
		{
			if (_mode_sigh.valid())
				Signal_transmitter(_mode_sigh).submit();
		}

		/**
		 * Deliver sync signal to the client's virtual frame buffer
		 */
		void submit_sync()
		{
			_framebuffer_session_component.submit_sync();
		}

		void forget(Gui_session &session)
		{
			if (_forwarded_focus == &session)
				_forwarded_focus = nullptr;
		}


		/*********************************
		 ** GUI session interface **
		 *********************************/

		Framebuffer::Session_capability framebuffer_session() override {
			return _framebuffer_session_cap; }

		Input::Session_capability input_session() override {
			return _input_session_cap; }

		View_handle create_view(View_handle parent_handle) override;

		void destroy_view(View_handle handle) override;

		View_handle view_handle(View_capability view_cap, View_handle handle) override;

		View_capability view_capability(View_handle handle) override;

		void release_view_handle(View_handle handle) override;

		Dataspace_capability command_dataspace() override { return _command_ds.cap(); }

		void execute() override;

		Framebuffer::Mode mode() override;

		void mode_sigh(Signal_context_capability sigh) override { _mode_sigh = sigh; }

		void buffer(Framebuffer::Mode mode, bool use_alpha) override;

		void focus(Capability<Gui::Session> session_cap) override;


		/*******************************
		 ** Buffer_provider interface **
		 *******************************/

		Dataspace_capability realloc_buffer(Framebuffer::Mode mode, bool use_alpha) override;
};

#endif /* _GUI_SESSION_H_ */
