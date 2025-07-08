/*
 * \brief  Wrapper around 'Sandboxed_runtime' for simple applications
 * \author Norman Feske
 * \date   2023-03-24
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__DIALOG__RUNTIME_H_
#define _INCLUDE__DIALOG__RUNTIME_H_

#include <dialog/sandboxed_runtime.h>

namespace Dialog { struct Runtime; }


class Dialog::Runtime : private Sandbox::State_handler
{
	private:

		Env       &_env;
		Allocator &_alloc;

		Sandbox _sandbox { _env, *this };

		Sandboxed_runtime _runtime { _env, _alloc, _sandbox };

		void _generate_sandbox_config(Generator &g) const
		{
			g.node("report", [&] () {
				g.attribute("child_ram",  "yes");
				g.attribute("child_caps", "yes");
				g.attribute("delay_ms", 20*1000);
			});
			g.node("parent-provides", [&] () {

				auto service_node = [&] (char const *name) {
					g.node("service", [&] () {
						g.attribute("name", name); }); };

				service_node("ROM");
				service_node("CPU");
				service_node("PD");
				service_node("LOG");
				service_node("Gui");
				service_node("Timer");
				service_node("Report");
				service_node("File_system");
			});

			_runtime.gen_start_nodes(g);
		}

		void _update_sandbox_config()
		{
			Generated_node { _alloc, { 4000 }, "config",
				[&] (Generator &g) { _generate_sandbox_config(g); }
			}.node.with_result(
				[&] (Node const &node) { _sandbox.apply_config(node); },
				[&] (Buffer_error) { warning("sandbox config unexpectedly large"); }
			);
		}

		/**
		 * Sandbox::State_handler
		 */
		void handle_sandbox_state() override
		{
			/* obtain current sandbox state */
			Generated_node const state { _alloc, { 4000 }, "state", [&] (Generator &g) {
				_sandbox.generate_state_report(g); } };

			bool reconfiguration_needed = false;

			state.node.with_result(
				[&] (Node const &node) {
					if (_runtime.apply_sandbox_state(node))
						reconfiguration_needed = true; },
				[&] (Buffer_error) {
					warning("sandbox state unexpectely large");
				});

			if (reconfiguration_needed)
				_update_sandbox_config();
		}

	public:

		Runtime(Env &env, Allocator &alloc) : _env(env), _alloc(alloc) { }

		struct View : Sandboxed_runtime::View
		{
			View(Runtime &runtime, Top_level_dialog &dialog, auto &&... args)
			:
				Sandboxed_runtime::View(runtime._runtime, dialog, args...)
			{
				runtime._update_sandbox_config();
			}
		};

		template <typename T>
		struct Event_handler : Sandboxed_runtime::Event_handler<T>
		{
			Event_handler(Runtime &runtime, T &obj, void (T::*member)(Event const &))
			: Sandboxed_runtime::Event_handler<T>(runtime._runtime, obj, member) { }
		};

		void update_view_config() { _update_sandbox_config(); }
};

#endif /* _INCLUDE__DIALOG__RUNTIME_H_ */
