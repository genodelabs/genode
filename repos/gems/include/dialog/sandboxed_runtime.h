/*
 * \brief  Runtime for hosting GUI dialogs in child components
 * \author Norman Feske
 * \date   2023-03-24
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__DIALOG__SANDBOXED_RUNTIME_H_
#define _INCLUDE__DIALOG__SANDBOXED_RUNTIME_H_

#include <util/dictionary.h>
#include <util/color.h>
#include <os/dynamic_rom_session.h>
#include <base/session_object.h>
#include <report_session/report_session.h>
#include <sandbox/sandbox.h>
#include <dialog/types.h>

namespace Dialog { struct Sandboxed_runtime; }


class Dialog::Sandboxed_runtime : Noncopyable
{
	public:

		class View;

		struct Event_handler_base : Interface, Noncopyable
		{
			virtual void handle_event(Event const &event) = 0;
		};

		template <typename T> class Event_handler;

		using Start_name = String<128>;

	private:

		Env       &_env;
		Allocator &_alloc;
		Sandbox   &_sandbox;

		struct Optional_event_handler
		{
			Event_handler_base *_ptr;

			void handle_event(Event const &event)
			{
				if (_ptr) _ptr->handle_event(event);
			}

		} _optional_event_handler { };

		using Views = Dictionary<View, Top_level_dialog::Name>;

		Event::Seq_number _global_seq_number { 1 };

		Views _views { };

		struct Gui_session;
		struct Report_session;

		using Gui_service    = Sandbox::Local_service<Gui_session>;
		using Rom_service    = Sandbox::Local_service<Dynamic_rom_session>;
		using Report_service = Sandbox::Local_service<Report_session>;

		void _handle_gui_service();
		void _handle_rom_service();
		void _handle_report_service();

		struct Service_handler : Sandbox::Local_service_base::Wakeup
		{
			Sandboxed_runtime &_runtime;

			using Member = void (Sandboxed_runtime::*) ();
			Member _member;

			void wakeup_local_service() override
			{
				(_runtime.*_member)();
			}

			Service_handler(Sandboxed_runtime &runtime, Member member)
			: _runtime(runtime), _member(member) { }
		};

		Service_handler _gui_handler    { *this, &Sandboxed_runtime::_handle_gui_service };
		Service_handler _rom_handler    { *this, &Sandboxed_runtime::_handle_rom_service };
		Service_handler _report_handler { *this, &Sandboxed_runtime::_handle_report_service };

		Gui_service    _gui_service;
		Rom_service    _rom_service;
		Report_service _report_service;

		struct Menu_view_state
		{
			Start_name const name;
			Ram_quota  const initial_ram  { 4*1024*1024 };
			Cap_quota  const initial_caps { 100 };

			Ram_quota ram  = initial_ram;
			Cap_quota caps = initial_caps;

			unsigned version = 0;

			void trigger_restart()
			{
				version++;
				ram  = initial_ram;
				caps = initial_caps;
			}

			/**
			 * Adapt runtime state information to the child
			 *
			 * This method responds to RAM and cap-resource requests by increasing
			 * the resource quotas as needed.
			 *
			 * \param  child  child node of the sandbox state report
			 * \return true if runtime must be reconfigured so that the changes
			 *         can take effect
			 */
			bool apply_child_state_report(Xml_node const &child)
			{
				bool result = false;

				if (child.attribute_value("name", Start_name()) != name)
					return false;

				if (child.has_sub_node("ram") && child.sub_node("ram").has_attribute("requested")) {
					ram.value *= 2;
					result = true;
				}

				if (child.has_sub_node("caps") && child.sub_node("caps").has_attribute("requested")) {
					caps.value += 100;
					result = true;
				}

				return result;
			}

			void gen_start_node(Xml_generator &, Views const &) const;

		} _menu_view_state;

		class Report_session : public Session_object<Report::Session>
		{
			public:

				struct Handler : Interface, Genode::Noncopyable
				{
					virtual void handle_report() = 0;
				};

			private:

				Attached_ram_dataspace _client_ds;
				Attached_ram_dataspace _local_ds;

				Constructible<Xml_node> _xml { }; /* points inside _local_ds */

				Handler &_handler;


				/*******************************
				 ** Report::Session interface **
				 *******************************/

				Dataspace_capability dataspace() override { return _client_ds.cap(); }

				void submit(size_t length) override
				{
					size_t const num_bytes = min(_client_ds.size(), length);

					memcpy(_local_ds.local_addr<char>(), _client_ds.local_addr<char>(),
					       num_bytes);

					_xml.destruct();

					try { _xml.construct(_local_ds.local_addr<char>(), num_bytes); }
					catch (...) { }

					_handler.handle_report();
				}

				void response_sigh(Signal_context_capability) override { }

				size_t obtain_response() override { return 0; }

			public:

				template <typename... ARGS>
				Report_session(Env &env, Handler &handler,
				               Entrypoint &ep, Resources const &resources,
				               ARGS &&... args)
				:
					Session_object(ep, resources, args...),
					_client_ds(env.ram(), env.rm(), resources.ram_quota.value/2),
					_local_ds (env.ram(), env.rm(), resources.ram_quota.value/2),
					_handler(handler)
				{ }

				template <typename FN>
				void with_xml(FN const &fn) const
				{
					if (_xml.constructed())
						fn(*_xml);
					else
						fn(Xml_node("<empty/>"));
				}
		};

		template <typename T>
		struct Hover_handler : Report_session::Handler
		{
			T &_obj;
			void (T::*_member) ();

			Hover_handler(T &obj, void (T::*member)())
			: _obj(obj), _member(member) { }

			void handle_report() override
			{
				(_obj.*_member)();
			}
		};

		Top_level_dialog::Name _hovered_dialog { };

		Hover_handler<Sandboxed_runtime> _hover_handler {
			*this, &Sandboxed_runtime::_handle_hover };

		void _handle_hover();

		Constructible<Report_session> _hover_report_session { };

		Event::Seq_number _hover_seq_number { };

	public:

		struct Attr { Start_name name; };

		Sandboxed_runtime(Env &, Allocator &, Sandbox &,
		                  Attr const &attr = { "view" });

		/**
		 * Respond to sandbox state changes
		 *
		 * \return true  if the sandbox configuration needs to be updated
		 */
		bool apply_sandbox_state(Xml_node const &);

		void gen_start_nodes(Xml_generator &) const;
};


class Dialog::Sandboxed_runtime::View : private Views::Element
{
	private:

		/* needed for privately inheriting 'Views::Element' */
		friend class Dictionary<View, Top_level_dialog::Name>;
		friend class Avl_node<View>;
		friend class Avl_tree<View>;
		friend class Sandboxed_runtime;

		Env &_env;

		Sandboxed_runtime &_runtime;

		Top_level_dialog &_dialog;

		bool _dialog_hovered = false;

		/* sequence numbers to correlate hover info with click/clack events */
		Constructible<Event::Seq_number> _click_seq_number { };
		Constructible<Event::Seq_number> _clack_seq_number { };

		bool _click_delivered = false; /* used to deliver each click only once */

		bool _dragged() const
		{
			return _click_seq_number.constructed()
			   && *_click_seq_number == _runtime._global_seq_number
			   &&  _click_delivered;
		}

		bool _hover_observable_without_click = false;

		void _with_dialog_hover(auto const &fn) const
		{
			bool done = false;

			if (_runtime._hover_report_session.constructed())
				_runtime._hover_report_session->with_xml([&] (Xml_node const &hover) {
					hover.with_optional_sub_node("dialog", [&] (Xml_node const &dialog) {
						fn(dialog);
						done = true; }); });

			if (!done)
				fn(Xml_node("<empty/>"));
		}

		struct Rom_producer : Dynamic_rom_session::Xml_producer
		{
			View const &_view;

			Rom_producer(View const &view)
			:
				Dynamic_rom_session::Xml_producer("dialog"),
				_view(view)
			{ }

			void produce_xml(Xml_generator &xml) override
			{
				_view._with_dialog_hover([&] (Xml_node const &hover) {

					Event::Dragged const dragged { _view._dragged() };

					bool const supply_hover = _view._hover_observable_without_click
					                       || dragged.value;

					static Xml_node omitted_hover("<hover/>");

					At const at { _view._runtime._global_seq_number,
					              supply_hover ? hover : omitted_hover };

					Scope<> top_level_scope(xml, at, dragged, { _view._dialog.name });

					_view._dialog.view(top_level_scope);
				});
			}
		} _dialog_producer { *this };

		Dynamic_rom_session _dialog_rom_session {
			_env.ep(), _env.ram(), _env.rm(), _dialog_producer };

		void _gen_menu_view_dialog(Xml_generator &) const;
		void _gen_menu_view_routes(Xml_generator &) const;

		void _handle_input_event(Input::Event const &);

		void _handle_hover();
		void _leave();
		void _try_handle_click_and_clack();

		Registry<Gui_session> _gui_sessions { };

	public:

		int      xpos       { };
		int      ypos       { };
		unsigned min_width  { };
		unsigned min_height { };
		bool     opaque     { };
		Color    background { };

		View(Sandboxed_runtime &runtime, Top_level_dialog &dialog)
		:
			Views::Element(runtime._views, dialog.name),
			_env(runtime._env), _runtime(runtime), _dialog(dialog)
		{ }

		~View();

		void refresh() { _dialog_rom_session.trigger_update(); }

		bool if_hovered(auto const &fn) const
		{
			bool result = false;
			if (_dialog_hovered)
				_with_dialog_hover([&] (Xml_node const &location) {
					result = fn(Hovered_at { Event::Seq_number { }, location }); });
			return result;
		}
};


template <typename T>
class Dialog::Sandboxed_runtime::Event_handler : Event_handler_base
{
	private:

		Sandboxed_runtime &_runtime;

		T &_obj;

		void (T::*_member) (Event const &);
		void handle_event(Event const &event) override { (_obj.*_member)(event); }

	public:

		Event_handler(Sandboxed_runtime &runtime, T &obj, void (T::*member)(Event const &))
		:
			_runtime(runtime), _obj(obj), _member(member)
		{
			/* register event handler at runtime */
			_runtime._optional_event_handler._ptr = this;
		}

		~Event_handler() { _runtime._optional_event_handler._ptr = nullptr; }
};

#endif /* _INCLUDE__DIALOG__SANDBOXED_RUNTIME_H_ */
