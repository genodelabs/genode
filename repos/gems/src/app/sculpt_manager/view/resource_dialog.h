/*
 * \brief  Resource assignment dialog
 * \author Alexander Boettcher
 * \date   2020-07-22
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW__RESOURCE_DIALOG_H_
#define _VIEW__RESOURCE_DIALOG_H_

/* Genode includes */
#include <os/reporter.h>
#include <depot/archive.h>

/* local includes */
#include <xml.h>
#include <model/component.h>
#include <view/dialog.h>

namespace Sculpt { struct Resource_dialog; }


struct Sculpt::Resource_dialog : Noncopyable, Dialog
{
	Affinity::Space const _space;
	Affinity::Location    _location;
	Hoverable_item        _space_item { };


	Resource_dialog(Affinity::Space space, Affinity::Location location)
	: _space(space), _location(location)
	{ }

	void _gen_affinity_entry(Xml_generator &, Start_name const &) const;

	Hover_result hover(Xml_node hover_node) override
	{
		Dialog::Hover_result const hover_result = hover(hover_node);
		return hover_result;
	}

	template <typename... ARGS>
	Hover_result hover(Xml_node hover, ARGS &&... args)
	{
		Dialog::Hover_result const hover_result = Dialog::any_hover_changed(
			_space_item.match(hover, args..., "hbox", "float", "vbox", "hbox", "button", "name"));

		return hover_result;
	}

	Start_name start_name() const { return "cpus"; }

	void click(Component &);

	void generate(Xml_generator &xml) const override
	{
		_gen_affinity_entry(xml, start_name());
	}

	void reset() override
	{
		_space_item._hovered = Hoverable_item::Id();
		_location = Affinity::Location();
	}
};

#endif /* _VIEW__RESOURCE_DIALOG_H_ */
