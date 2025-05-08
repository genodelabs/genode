/*
 * \brief  GUI session component
 * \author Norman Feske
 * \date   2017-11-16
 */

/*
 * Copyright (C) 2006-2024 Genode Labs GmbH
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
#include <os/dynamic_rom_session.h>
#include <gui_session/gui_session.h>

/* local includes */
#include <canvas.h>
#include <domain_registry.h>
#include <framebuffer_session.h>
#include <input_session.h>
#include <focus.h>
#include <view.h>

namespace Nitpicker {

	class Gui_session;
	class View;

	using Session_list = List<Gui_session>;
}


class Nitpicker::Gui_session : public  Session_object<Gui::Session>,
                               public  View_owner,
                               public  Buffer_provider,
                               private Session_list::Element,
                               private Dynamic_rom_session::Xml_producer,
                               private Input::Session_component::Action
{
	public:

		struct Action : Interface
		{
			/*
			 * \param rect  domain-specific panorama rectangle
			 */
			virtual void gen_capture_info(Xml_generator &xml, Rect rect) const = 0;

			virtual void exclusive_input_changed() = 0;
		};

	private:

		struct View_ref : Gui::View_ref
		{
			Weak_ptr<View> _weak_ptr;

			View_ids::Element id;

			View_ref(Weak_ptr<View> view, View_ids &ids)
			: _weak_ptr(view), id(*this, ids) { }

			View_ref(Weak_ptr<View> view, View_ids &ids, View_id id)
			: _weak_ptr(view), id(*this, ids, id) { }

			auto with_view(auto const &fn, auto const &missing_fn) -> decltype(missing_fn())
			{
				/*
				 * Release the lock before calling 'fn' to allow the nesting of
				 * 'with_view' calls. The locking aspect of the weak ptr is not
				 * needed here because the component is single-threaded.
				 */
				View *ptr = nullptr;
				{
					Locked_ptr<View> view(_weak_ptr);
					if (view.valid())
						ptr = view.operator->();
				}
				return ptr ? fn(*ptr) : missing_fn();
			}
		};

		friend class List<Gui_session>;

		using Gui::Session::Label;

		/*
		 * Noncopyable
		 */
		Gui_session(Gui_session const &);
		Gui_session &operator = (Gui_session const &);

		Env    &_env;
		Action &_action;

		Accounted_ram_allocator _ram;

		Resizeable_texture<Pixel> _texture { };

		Domain_registry::Entry const *_domain     = nullptr;
		View                         *_background = nullptr;

		bool _visible = true;

		Sliced_heap _session_alloc;

		Framebuffer::Session_component _framebuffer_session_component;

		void withdraw(Ram_quota q) { if (!try_withdraw(q)) throw Out_of_ram(); }
		void withdraw(Cap_quota q) { if (!try_withdraw(q)) throw Out_of_caps(); }

		bool const _input_session_accounted = (
			withdraw(Ram_quota{Input::Session_component::ev_ds_size()}), true );

		Input::Session_component _input_session_component { _env, *this };

		View_stack &_view_stack;

		Focus_updater &_focus_updater;
		Hover_updater &_hover_updater;

		Constructible<Dynamic_rom_session> _info_rom { };

		View &_pointer_origin;

		View &_builtin_background;

		List<Session_view_list_elem> _view_list { };

		/*
		 * Slab allocator that includes an initial block as member
		 */
		template <size_t BLOCK_SIZE>
		struct Initial_slab_block { uint8_t buf[BLOCK_SIZE]; };
		template <typename T, size_t BLOCK_SIZE>
		struct Slab : private Initial_slab_block<BLOCK_SIZE>, Tslab<T, BLOCK_SIZE>
		{
			Slab(Allocator &block_alloc)
			: Tslab<T, BLOCK_SIZE>(block_alloc, Initial_slab_block<BLOCK_SIZE>::buf) { };
		};

		Slab<View,     4000> _view_alloc     { _session_alloc };
		Slab<View_ref, 4000> _view_ref_alloc { _session_alloc };

		bool const _provides_default_bg;

		/* size of currently allocated virtual framebuffer, in bytes */
		size_t _buffer_size = 0;

		Attached_ram_dataspace _command_ds { _env.ram(), _env.rm(),
		                                     sizeof(Command_buffer) };

		bool const _command_buffer_accounted = (
			withdraw(Ram_quota{align_addr(sizeof(Session::Command_buffer), 12)}), true );

		Command_buffer &_command_buffer = *_command_ds.local_addr<Command_buffer>();

		View_ids _view_ids { };

		Reporter &_focus_reporter;

		Gui_session *_forwarded_focus = nullptr;

		/**
		 * Calculate session-local coordinate to position within panorama
		 *
		 * \param pos   coordinate in session-local coordinate system
		 * \param rect  geometry within panorama
		 */
		Point _phys_pos(Point pos, Rect panorama) const
		{
			return _domain ? _domain->phys_pos(pos, panorama) : Point(0, 0);
		}

		void _execute_command(Command const &);

		void _destroy_view(View &);

		void _adopt_new_view(View &);

		View_result _create_view_and_ref(View_id, View_attr const &attr, auto const &);

		auto _with_view(View_id id, auto const &fn, auto const &missing_fn)
		-> decltype(missing_fn())
		{
			return _view_ids.apply<View_ref>(id,
				[&] (View_ref &view_ref) {
					return view_ref.with_view(
						[&] (View &view)        { return fn(view); },
						[&] /* view vanished */ { return missing_fn(); });
				},
				[&] /* ID does not exist */ { return missing_fn(); });
		}

		/**
		 * Dynamic_rom_session::Xml_producer interface
		 */
		void produce_xml(Xml_generator &) override;

		bool _exclusive_input_requested = false;

		/**
		 * Input::Session_component::Action interface
		 */
		void exclusive_input_requested(bool const requested) override
		{
			bool const orig = _exclusive_input_requested;
			_exclusive_input_requested = requested;
			if (orig != requested)
				_action.exclusive_input_changed();
		}

	public:

		Gui_session(Env                   &env,
		            Action                &action,
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
			Xml_producer("panorama"),
			_env(env), _action(action),
			_ram(env.ram(), _ram_quota_guard(), _cap_quota_guard()),
			_session_alloc(_ram, env.rm()),
			_framebuffer_session_component(env.ep(), view_stack, *this, *this),
			_view_stack(view_stack),
			_focus_updater(focus_updater), _hover_updater(hover_updater),
			_pointer_origin(pointer_origin),
			_builtin_background(builtin_background),
			_provides_default_bg(provides_default_bg),
			_focus_reporter(focus_reporter)
		{ }

		~Gui_session()
		{
			while (_view_ids.apply_any<View_ref>([&] (View_ref &view_ref) {
				destroy(_view_ref_alloc, &view_ref); }));

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

		bool uses_alpha() const override { return _texture.alpha(); }

		unsigned layer() const override { return _domain ? _domain->layer() : ~0U; }

		bool origin_pointer() const override { return _domain && _domain->origin_pointer(); }

		/**
		 * Return input mask value at specified buffer position
		 */
		bool input_mask_at(Point const p) const override
		{
			bool result = false;
			_texture.with_input_mask([&] (Const_byte_range_ptr const &bytes) {

				unsigned const x = p.x % _texture.size().w,
				               y = p.y % _texture.size().h;

				size_t const offset = y*_texture.size().w + x;
				if (offset < bytes.num_bytes)
					result = bytes.start[offset];
			});
			return result;
		}

		void submit_input_event(Input::Event e) override;

		bool exclusive_input_requested() const override
		{
			return _exclusive_input_requested;
		}

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

		void reset_domain() { _domain = nullptr; }

		/**
		 * Set session domain according to the list of configured policies
		 *
		 * Select the policy that matches the label. If multiple policies
		 * match, select the one with the largest number of characters.
		 */
		void apply_session_policy(Xml_node const &config, Domain_registry const &);

		void destroy_all_views();

		/**
		 * Deliver mode-change signal to client
		 */
		void notify_mode_change()
		{
			if (_info_rom.constructed())
				_info_rom->trigger_update();
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

		Point panning() const { return _texture.panning; }


		/***************************
		 ** GUI session interface **
		 ***************************/

		Framebuffer::Session_capability framebuffer() override {
			return _framebuffer_session_component.cap(); }

		Input::Session_capability input() override {
			return _input_session_component.cap(); }

		Info_result info() override
		{
			if (!_info_rom.constructed()) {
				Cap_quota const needed_caps { 2 };
				if (!try_withdraw(needed_caps))
					return Info_error::OUT_OF_CAPS;

				bool out_of_caps = false, out_of_ram = false;
				try {
					Dynamic_rom_session::Content_producer &rom_producer = *this;
					_info_rom.construct(_env.ep(), _ram, _env.rm(), rom_producer);
				}
				catch (Out_of_ram)  { out_of_ram  = true; }
				catch (Out_of_caps) { out_of_caps = true; }

				if (out_of_ram || out_of_ram) {
					replenish(needed_caps);
					if (out_of_ram)  return Info_error::OUT_OF_RAM;
					if (out_of_caps) return Info_error::OUT_OF_CAPS;
				}
			}
			return _info_rom->cap();
		}

		View_result view(View_id, View_attr const &attr) override;

		Child_view_result child_view(View_id, View_id, View_attr const &attr) override;

		void destroy_view(View_id) override;

		Associate_result associate(View_id, View_capability) override;

		View_capability_result view_capability(View_id) override;

		void release_view_id(View_id) override;

		Dataspace_capability command_dataspace() override { return _command_ds.cap(); }

		void execute() override;

		Buffer_result buffer(Framebuffer::Mode) override;

		void focus(Capability<Gui::Session>) override;


		/*******************************
		 ** Buffer_provider interface **
		 *******************************/

		Dataspace_capability realloc_buffer(Framebuffer::Mode mode) override;

		void blit(Rect from, Point to) override { _texture.blit(from, to); }

		void panning(Point pos) override { _texture.panning = pos; }
};

#endif /* _GUI_SESSION_H_ */
