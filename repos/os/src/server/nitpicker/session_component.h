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

#ifndef _SESSION_COMPONENT_H_
#define _SESSION_COMPONENT_H_

/* Genode includes */
#include <util/list.h>
#include <base/rpc_server.h>
#include <os/session_policy.h>
#include <os/reporter.h>
#include <os/pixel_rgb565.h>
#include <nitpicker_session/nitpicker_session.h>
#include <base/allocator_guard.h>

/* local includes */
#include "canvas.h"
#include "domain_registry.h"
#include "framebuffer_session.h"
#include "input_session.h"
#include "focus.h"
#include "chunky_texture.h"
#include "view_component.h"

namespace Nitpicker {

	class Visibility_controller;
	class Session_component;
	class View_component;

	typedef List<Session_component> Session_list;
}


struct Nitpicker::Visibility_controller : Interface
{
	typedef Session::Label Suffix;

	virtual void hide_matching_sessions(Session_label const &, Suffix const &) = 0;

	virtual void show_matching_sessions(Session_label const &, Suffix const &) = 0;
};


class Nitpicker::Session_component : public  Rpc_object<Session>,
                                     public  View_owner,
                                     public  Buffer_provider,
                                     private Session_list::Element
{
	private:

		friend class List<Session_component>;

		/*
		 * Noncopyable
		 */
		Session_component(Session_component const &);
		Session_component &operator = (Session_component const &);

		Env &_env;

		Session_label const _label;

		Domain_registry::Entry const *_domain     = nullptr;
		Texture_base           const *_texture    = nullptr;
		View_component               *_background = nullptr;

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

		Allocator_guard _session_alloc;

		Framebuffer::Session &_framebuffer;

		Framebuffer::Session_component _framebuffer_session_component;

		Input::Session_component _input_session_component { _env };

		View_stack &_view_stack;

		Font const &_font;

		Focus_updater &_focus_updater;

		Signal_context_capability _mode_sigh { };

		View_component &_pointer_origin;

		View_component &_builtin_background;

		List<Session_view_list_elem> _view_list { };

		Tslab<View_component, 4000> _view_alloc { &_session_alloc };

		/* capabilities for sub sessions */
		Framebuffer::Session_capability _framebuffer_session_cap;
		Input::Session_capability       _input_session_cap;

		bool const _provides_default_bg;

		/* size of currently allocated virtual framebuffer, in bytes */
		size_t _buffer_size = 0;

		Attached_ram_dataspace _command_ds { _env.ram(), _env.rm(),
		                                     sizeof(Command_buffer) };

		Command_buffer &_command_buffer = *_command_ds.local_addr<Command_buffer>();

		typedef Handle_registry<View_handle, View_component> View_handle_registry;

		View_handle_registry _view_handle_registry;

		Reporter &_focus_reporter;

		Visibility_controller &_visibility_controller;

		Session_component *_forwarded_focus = nullptr;

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

		void _release_buffer();

		/**
		 * Helper for performing sanity checks in OP_TO_FRONT and OP_TO_BACK
		 *
		 * We have to check for the equality of both the specified view and
		 * neighbor. If both arguments refer to the same view, the creation of
		 * locked pointers for both views would result in a deadlock.
		 */
		bool _views_are_equal(View_handle, View_handle);

		void _execute_command(Command const &);

		void _destroy_view(View_component &);

	public:

		Session_component(Env                   &env,
		                  Session_label   const &label,
		                  View_stack            &view_stack,
		                  Font            const &font,
		                  Focus_updater         &focus_updater,
		                  View_component        &pointer_origin,
		                  View_component        &builtin_background,
		                  Framebuffer::Session  &framebuffer,
		                  bool                   provides_default_bg,
		                  Allocator             &session_alloc,
		                  size_t                 ram_quota,
		                  Reporter              &focus_reporter,
		                  Visibility_controller &visibility_controller)
		:
			_env(env),
			_label(label),
			_session_alloc(&session_alloc, ram_quota),
			_framebuffer(framebuffer),
			_framebuffer_session_component(view_stack, *this, framebuffer, *this),
			_view_stack(view_stack), _font(font), _focus_updater(focus_updater),
			_pointer_origin(pointer_origin),
			_builtin_background(builtin_background),
			_framebuffer_session_cap(_env.ep().manage(_framebuffer_session_component)),
			_input_session_cap(_env.ep().manage(_input_session_component)),
			_provides_default_bg(provides_default_bg),
			_view_handle_registry(_session_alloc),
			_focus_reporter(focus_reporter),
			_visibility_controller(visibility_controller)
		{
			_session_alloc.upgrade(ram_quota);
		}

		~Session_component()
		{
			_env.ep().dissolve(_framebuffer_session_component);
			_env.ep().dissolve(_input_session_component);

			destroy_all_views();

			_release_buffer();
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
			return static_cast<Session_component const &>(*owner)._domain == _domain;
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

		Texture_base const *texture() const override { return _texture; }

		bool uses_alpha() const override { return _texture && _uses_alpha; }

		unsigned layer() const override { return _domain ? _domain->layer() : ~0UL; }

		bool origin_pointer() const override { return _domain && _domain->origin_pointer(); }

		/**
		 * Return input mask value at specified buffer position
		 */
		unsigned char input_mask_at(Point p) const override
		{
			if (!_input_mask || !_texture) return 0;

			/* check boundaries */
			if ((unsigned)p.x() >= _texture->size().w()
			 || (unsigned)p.y() >= _texture->size().h())
				return 0;

			return _input_mask[p.y()*_texture->size().w() + p.x()];
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

		void upgrade_ram_quota(size_t ram_quota) { _session_alloc.upgrade(ram_quota); }

		/**
		 * Deliver sync signal to the client's virtual frame buffer
		 */
		void submit_sync()
		{
			_framebuffer_session_component.submit_sync();
		}

		void forget(Session_component &session)
		{
			if (_forwarded_focus == &session)
				_forwarded_focus = nullptr;
		}


		/*********************************
		 ** Nitpicker session interface **
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

		void focus(Capability<Nitpicker::Session> session_cap) override;

		void session_control(Label suffix, Session_control control) override;


		/*******************************
		 ** Buffer_provider interface **
		 *******************************/

		Buffer *realloc_buffer(Framebuffer::Mode mode, bool use_alpha) override;
};

#endif /* _SESSION_COMPONENT_H_ */
