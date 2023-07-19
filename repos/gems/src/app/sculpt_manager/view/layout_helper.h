/*
 * \brief  GUI layout helper
 * \author Norman Feske
 * \date   2022-05-20
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW__LAYOUT_HELPER_H_
#define _VIEW__LAYOUT_HELPER_H_

#include <view/dialog.h>


/**
 * Inflate vertical spacing using an invisble button
 */
template <typename ID>
static void gen_item_vspace(Genode::Xml_generator &xml, ID const &id)
{
	using namespace Sculpt;

	gen_named_node(xml, "button", id, [&] () {
		xml.attribute("style", "invisible");
		xml.node("label", [&] () {
			xml.attribute("text", " ");
			xml.attribute("font", "title/regular");
		});
	});
}


#endif /* _VIEW__LAYOUT_HELPER_H_ */
