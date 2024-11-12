/*
 * \brief  Internal Representation of GUI panorama
 * \author Norman Feske
 * \date   2024-11-12
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _PANORAMA_H_
#define _PANORAMA_H_

/* local includes */
#include <types.h>

namespace Window_layouter { class Panorama; }


struct Window_layouter::Panorama
{
	Allocator &_alloc;

	Rect rect { };

	struct Capture;
	using Captures = List_model<Capture>;

	Captures _captures { };

	struct Capture : Captures::Element
	{
		Name const name { };

		Rect rect { };

		Capture(Name const name) : name(name) { }

		/**
		 * List_model::Element
		 */
		void update(Xml_node const &node) { rect = Rect::from_xml(node); }

		/**
		 * List_model::Element
		 */
		bool matches(Xml_node const &node) const
		{
			return name_from_xml(node) == name;
		}

		/**
		 * List_model::Element
		 */
		static bool type_matches(Xml_node const &node)
		{
			return node.has_type("capture");
		}
	};

	Panorama(Allocator &alloc) : _alloc(alloc) { }

	~Panorama() { update_from_xml("<empty/>"); }

	void update_from_xml(Xml_node const &gui_info)
	{
		rect = Rect::from_xml(gui_info);

		_captures.update_from_xml(gui_info,

			[&] (Xml_node const &node) -> Capture & {
				return *new (_alloc) Capture(name_from_xml(node)); },

			[&] (Capture &capture) { destroy(_alloc, &capture); },

			[&] (Capture &capture, Xml_node const &node) { capture.update(node); }
		);
	}

	void with_leftmost_captured_rect(int const min_x, auto const &fn) const
	{
		Rect rect { };
		int max_x = 1000000;
		_captures.for_each([&] (Capture const &capture) {
			if (capture.rect.x1() >= min_x && capture.rect.x1() < max_x) {
				max_x = capture.rect.x1();
				rect  = capture.rect;
			}
		});
		if (rect.valid())
			fn(rect);
	};

	void with_matching_capture_rect(Xml_node const &policy, auto const &fn) const
	{
		Rect rect { };
		_captures.for_each([&] (Capture const &capture) {
			if (!rect.valid() && !Xml_node_label_score(policy, capture.name).conflict())
				rect = capture.rect; });
		if (rect.valid())
			fn(rect);
	}
};

#endif /* _PANORAMA_H_ */
