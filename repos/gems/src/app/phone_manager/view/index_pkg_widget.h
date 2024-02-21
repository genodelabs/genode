/*
 * \brief  Widget for presenting pkg details and the option for installation
 * \author Norman Feske
 * \date   2023-03-22
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW__INDEX_PKG_WIDGET_H_
#define _VIEW__INDEX_PKG_WIDGET_H_

#include <model/nic_state.h>
#include <view/depot_users_widget.h>
#include <view/component_info_widget.h>

namespace Sculpt { struct Index_pkg_widget; }


struct Sculpt::Index_pkg_widget : Widget<Float>
{
	using User_properties = Depot_users_widget::User_properties;

	Hosted<Float, Vbox, Float, Deferred_action_button> _install { Id { "install" } };

	void view(Scope<Float> &s, Component const &component,
	          User_properties const &properties, Nic_state const &nic_state) const
	{
		if (!component.blueprint_info.known || component.blueprint_info.ready_to_deploy())
			return;

		s.sub_scope<Vbox>([&] (Scope<Float, Vbox> &s) {

			/*
			 * Package is installed but content is missing
			 *
			 * This can happen when the pkg's runtime is inconsistent with
			 * the content contained in the pkg's archives.
			 */
			if (component.blueprint_info.incomplete()) {
				s.sub_scope<Small_vgap>();
				s.sub_scope<Annotation>(component.path);
				s.sub_scope<Small_vgap>();
				s.sub_scope<Label>("installed but incomplete");

				if (nic_state.ready()) {
					s.sub_scope<Small_vgap>();

					auto const text = properties.public_key
					                ? " Reattempt Install "
					                : " Reattempt Install without Verification ";

					s.sub_scope<Float>([&] (Scope<Float, Vbox, Float> &s) {
						s.widget(_install, [&] (Scope<Button> &s) {
							s.sub_scope<Label>(text); }); });
				}
				s.sub_scope<Small_vgap>();
			}

			/*
			 * Package is missing but can be installed
			 */
			else if (component.blueprint_info.uninstalled() && nic_state.ready()) {

				s.widget(Hosted<Float, Vbox, Component_info_widget> { Id { "info" } }, component);

				auto const text = properties.public_key
				                ? " Install "
				                : " Install without Verification ";

				s.sub_scope<Vgap>();
				s.sub_scope<Float>([&] (Scope<Float, Vbox, Float> &s) {
					s.widget(_install, [&] (Scope<Button> &s) {
						s.sub_scope<Label>(text); }); });
				s.sub_scope<Vgap>();
			}

			/*
			 * Package is missing and we cannot do anything about it
			 */
			else if (component.blueprint_info.uninstalled()) {
				s.sub_scope<Vgap>();
				s.sub_scope<Annotation>(component.path);
				s.sub_scope<Vgap>();
				s.sub_scope<Label>("not installed");
				s.sub_scope<Vgap>();
			}
		});
	}

	void click(Clicked_at const &at)
	{
		_install.propagate(at);
	}

	void clack(Clacked_at const &at, auto const install_fn)
	{
		_install.propagate(at, install_fn);
	}
};

#endif /* _VIEW__INDEX_PKG_WIDGET_H_ */
