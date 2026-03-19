/*
 * \brief  Utility for presenting runtime diagnostics
 * \author Norman Feske
 * \date   2026-03-19
 */

/*
 * Copyright (C) 2026 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW__RUNTIME_DIAG_H_
#define _VIEW__RUNTIME_DIAG_H_

#include <view/dialog.h>
#include <model/runtime_config.h>

namespace Sculpt {

	static inline void view_runtime_diag(Scope<> &, Allocator &, Runtime_config const &);
}


void Sculpt::view_runtime_diag(Scope<> &s, Allocator &alloc, Runtime_config const &runtime)
{
	/*
	 * Collect messages in registry, avoiding duplicates
	 */
	using Message = String<64>;
	using Registered_message = Registered_no_delete<Message>;
	Registry<Registered_message> messages { };

	auto add_message = [&] (Message const &new_message)
	{
		bool already_exists = false;
		messages.for_each([&] (Registered_message const &message) {
			if (message == new_message)
				already_exists = true; });

		if (!already_exists)
			new (alloc) Registered_message(messages, new_message);
	};

	runtime.for_each_stalled([&] (Node const &stalled) {
		Start_name const name = stalled.attribute_value("name", Start_name());
		stalled.with_optional_sub_node("deploy", [&] (Node const &deploy) {

			deploy.with_optional_sub_node("pkg_corrupt", [&] (Node const &) {
				auto const pkg = deploy.attribute_value("pkg", Depot::Archive::Path());
				add_message({ "corrupt ", pkg }); });

			deploy.with_optional_sub_node("pkg_missing", [&] (Node const &) {
				auto const pkg = deploy.attribute_value("pkg", Depot::Archive::Path());
				add_message({ "missing ", pkg }); });

			deploy.with_optional_sub_node("deps", [&] (Node const &dep) {
				dep.for_each_sub_node([&] (Node const &node) {
					Start_name const server = node.attribute_value("name", Start_name());
					add_message({ name, " requires ", server }); }); });
		});
	});

	/*
	 * Generate dialog elements, drop consumed messages from the registry
	 */
	messages.for_each([&] (Registered_message &message) {
		s.sub_scope<Left_annotation>(message);
		destroy(alloc, &message);
	});
}

#endif /* _VIEW__RUNTIME_DIAG_H_ */
