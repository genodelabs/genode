/*
 * \brief   GUI session
 * \author  Stefan Kalkowski
 * \date    2013-04-17
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */


#ifndef _GUI_SESSION_COMPONENT_H_
#define _GUI_SESSION_COMPONENT_H_

/* Genode includes */
#include <cpu/cpu_state.h>
#include <base/rpc_server.h>
#include <vmm_gui_session/client.h>

/* Qt4 includes */
#include <qnitpickerviewwidget/qnitpickerviewwidget.h>

/* local includes */
#include "control_bar.h"
#include "main_window.h"

namespace Vmm_gui {

	class Session_component : public Genode::Rpc_object<Session>
	{
		private:

			QNitpickerViewWidget &_view_widget;
			Control_bar          &_control_bar;
			Register_widget      &_reg_widget;

		public:

			/**
			 * Constructor
			 */
			Session_component(QNitpickerViewWidget &view_widget,
			                  Control_bar &control_bar,
			                  Register_widget &reg_widget)
			: _view_widget(view_widget), _control_bar(control_bar),
			  _reg_widget(reg_widget) { }

			void show_view(Nitpicker::View_capability cap, int w, int h) {
				_view_widget.setNitpickerView(cap, 0, 0, w, h); }

			void play_resume_sigh(Genode::Signal_context_capability cap) {
				_control_bar.play_sigh(cap); }
			void stop_sigh(Genode::Signal_context_capability cap) {
				_control_bar.stop_sigh(cap); }
			void power_sigh(Genode::Signal_context_capability cap) {
				_control_bar.power_sigh(cap); }
			void bomb_sigh(Genode::Signal_context_capability cap) {
				_control_bar.bomb_sigh(cap); }
			void fullscreen_sigh(Genode::Signal_context_capability cap) {}

			void set_state(Vm_state const &vm_state) {
				_reg_widget.set_state((Genode::Cpu_state_modes*)vm_state.base()); }
	};


	typedef Genode::Root_component<Session_component, Genode::Single_client> Root_component;


	class Root : public Root_component
	{
		private:

			QNitpickerViewWidget &_nitpicker_view_widget;
			Control_bar          &_control_bar;
			Register_widget      &_reg_widget;

		protected:

			Session_component *_create_session(const char *args)
			{
				return new (md_alloc()) Session_component(_nitpicker_view_widget,
				                                          _control_bar, _reg_widget);
			}

		public:

			Root(Genode::Rpc_entrypoint *session_ep,
				 Genode::Allocator      *md_alloc,
				 QNitpickerViewWidget   &nitpicker_view_widget,
				 Control_bar            &control_bar,
				 Register_widget        &reg_widget)
			: Root_component(session_ep, md_alloc),
			  _nitpicker_view_widget(nitpicker_view_widget),
			  _control_bar(control_bar), _reg_widget(reg_widget){ }
	};
}

#endif /* _GUI_SESSION_H_ */
