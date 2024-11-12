/*
 * \brief  List of displays
 * \author Norman Feske
 * \date   2024-11-12
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DISPLAY_LIST_H_
#define _DISPLAY_LIST_H_

/* local includes */
#include <panorama.h>

namespace Window_layouter { class Display; }


struct Window_layouter::Display : List_model<Display>::Element
{
	Name const name;

	struct Attr
	{
		Rect rect;     /* within panorama */
		bool occupied; /* true if occupied by a screen */

	} attr { };

	Display(Name const &name) : name(name) { }

	/**
	 * List_model::Element
	 */
	void update(Panorama const &panorama, Xml_node const &node)
	{
		/* import explicitly configured panorama position */
		attr.rect = Rect::from_xml(node);

		/* assign panorama rect according to matching display <capture> policy */
		node.for_each_sub_node("capture", [&] (Xml_node const &policy) {
			if (!attr.rect.valid())
				panorama.with_matching_capture_rect(policy, [&] (Rect r) {
					attr.rect = r; }); });
	}

	/**
	 * List_model::Element
	 */
	bool matches(Xml_node node) const
	{
		return name_from_xml(node) == name;
	}

	/**
	 * List_model::Element
	 */
	static bool type_matches(Xml_node const &node)
	{
		return node.has_type("display");
	}
};



namespace Window_layouter { class Display_list; }


class Window_layouter::Display_list : Noncopyable
{
	private:

		Allocator &_alloc;

		List_model<Display> _displays { };

		Display::Attr _panorama_attr { }; /* fallback used if no display declared */

		void _update_from_xml(Xml_node const &node, auto const &update_fn)
		{
			_displays.update_from_xml(node,

				[&] (Xml_node const &node) -> Display & {
					return *new (_alloc) Display(name_from_xml(node)); },

				[&] (Display &display) { destroy(_alloc, &display); },

				update_fn
			);
		}

	public:

		Display_list(Allocator &alloc) : _alloc(alloc) { }

		~Display_list()
		{
			_update_from_xml(Xml_node("<empty/>"), [&] (auto &, auto &) { });
		}

		void update_from_xml(Panorama const &panorama, Xml_node const &node)
		{
			_panorama_attr.rect = panorama.rect;

			/* import display definitions and their panoramic positions */
			_update_from_xml(node, [&] (Display &display, Xml_node const &node) {
				display.update(panorama, node); });

			/* assign remaining unpositioned displays from left to right */
			int min_x = 0;
			_displays.for_each([&] (Display &display) {
				if (!display.attr.rect.valid())
					panorama.with_leftmost_captured_rect(min_x, [&] (Rect r) {
						display.attr.rect = r;
						min_x = r.x1() + 1; }); });

			/* assign still unpositioned displays to leftmost captured rect */
			_displays.for_each([&] (Display &display) {
				if (!display.attr.rect.valid())
					panorama.with_leftmost_captured_rect(0, [&] (Rect r) {
						display.attr.rect = r; }); });

			/* if nothing is captured assign the total panorama to the display */
			_displays.for_each([&] (Display &display) {
				if (!display.attr.rect.valid())
					display.attr.rect = panorama.rect; });
		}

		/**
		 * Call 'fn' with the panorama rectangle of the display named 'name'
		 */
		void with_display_attr(Name const &name, auto const &fn)
		{
			bool done = false;
			_displays.for_each([&] (Display &display) {
				if (!done && display.name == name) {
					fn(display.attr);
					done = true; } });

			if (!done)
				fn(_panorama_attr);
		}

		void mark_as_occupied(Rect const rect)
		{
			_displays.for_each([&] (Display &display) {
				if (rect == display.attr.rect)
					display.attr.occupied = true; });

			if (rect == _panorama_attr.rect)
				_panorama_attr.occupied = true;
		}

		void reset_occupied_flags()
		{
			_panorama_attr.occupied = false;
			_displays.for_each([&] (Display &display) {
				display.attr.occupied = false; });
		}
};

#endif /* _DISPLAY_LIST_H_ */
