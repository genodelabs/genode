/*
 * \brief  List of target areas where windows may be placed
 * \author Norman Feske
 * \date   2018-09-26
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TARGET_LIST_H_
#define _TARGET_LIST_H_

/* local includes */
#include <target.h>

namespace Window_layouter { class Target_list; }


class Window_layouter::Target_list
{
	private:

		Allocator &_alloc;

		Registry<Registered<Target> > _targets { };

		/*
		 * Keep information of the rules internally to reproduce this
		 * information in 'gen_screens'.
		 */
		Constructible<Buffered_xml> _rules { };

		/**
		 * Calculate layout and populate '_targets'
		 *
		 * \param row  true of 'node' is a row, false if 'node' is a column
		 */
		void _process_rec(Xml_node node, Rect avail, bool row,
		                  Target::Visible visible)
		{
			unsigned const avail_px = row ? avail.w() : avail.h();

			char const *sub_node_type = row ? "column" : "row";
			char const *px_size_attr  = row ? "width"  : "height";

			unsigned px_pos = row ? avail.x1() : avail.y1();

			unsigned const default_weight = 1;

			/*
			 * Determinine space reserved in pixels, the total weight, and
			 * number of weighted rows/columns.
			 */
			unsigned preserved_pixels = 0;
			unsigned total_weight     = 0;
			unsigned num_weighted     = 0;

			/* ignore weight if pixel size is provided */
			auto weight_attr_value = [&] (Xml_node node) {
				return node.has_attribute(px_size_attr)
				     ? 0 : node.attribute_value("weight", default_weight); };

			node.for_each_sub_node(sub_node_type, [&] (Xml_node child) {
				preserved_pixels += child.attribute_value(px_size_attr, 0U);
				total_weight     += weight_attr_value(child);
				num_weighted     += child.has_attribute(px_size_attr) ? 0 : 1;
			});

			if (preserved_pixels > avail_px) {
				warning("layout does not fit in available area ", avail_px, ":", node);
				return;
			}

			/* amount of pixels we can use for weighed columns */
			unsigned const weigthed_avail = avail_px - preserved_pixels;

			/*
			 * Calculate positions
			 */
			unsigned count_weighted = 0;
			unsigned used_weighted  = 0;
			node.for_each_sub_node(sub_node_type, [&] (Xml_node child) {

				auto calc_px_size = [&] () {

					unsigned const px = child.attribute_value(px_size_attr, 0U);
					if (px)
						return px;

					unsigned const weight = weight_attr_value(child);
					if (weight && total_weight)
						return (((weight << 16)*weigthed_avail)/total_weight) >> 16;

					return 0U;
				};

				bool const weighted = !child.has_attribute(px_size_attr);

				if (weighted)
					count_weighted++;

				/* true if target is the last weigthed column or row */
				bool const last_weighted = weighted
				                        && (count_weighted == num_weighted);

				unsigned const px_size = last_weighted
				                       ? (weigthed_avail - used_weighted)
				                       : calc_px_size();

				if (weighted)
					used_weighted += px_size;

				Rect const sub_rect = row ? Rect(Point(px_pos, avail.y1()),
				                                 Area(px_size, avail.h()))
				                          : Rect(Point(avail.x1(), px_pos),
				                                 Area(avail.w(), px_size));

				_process_rec(child, sub_rect, !row, visible);

				if (child.attribute_value("name", Target::Name()).valid())
					new (_alloc)
						Registered<Target>(_targets, child, sub_rect, visible);

				px_pos += px_size;
			});
		}

		static constexpr unsigned MAX_LAYER = 9999;

		/**
		 * Generate windows for the top-most layer, starting at 'min_layer'
		 *
		 * \return  layer that was processed by the method
		 */
		unsigned _gen_top_most_layer(Xml_generator &xml, unsigned min_layer,
		                             Assign_list const &assignments) const
		{
			/* search targets for next matching layer */
			unsigned layer = MAX_LAYER;
			_targets.for_each([&] (Target const &target) {
				if (target.layer() >= min_layer && target.layer() <= layer)
					layer = target.layer(); });

			/* visit all windows on the layer */
			assignments.for_each([&] (Assign const &assign) {

				if (!assign.visible())
					return;

				Target::Name const target_name = assign.target_name();

				/* search target by name */
				_targets.for_each([&] (Target const &target) {

					if (target.name() != target_name)
						return;

					if (target.layer() != layer)
						return;

					if (!target.visible())
						return;

					/* found target area, iterate though all assigned windows */
					assign.for_each_member([&] (Assign::Member const &member) {
						member.window.generate(xml); });
				});
			});

			return layer;
		}

	public:

		Target_list(Allocator &alloc) : _alloc(alloc) { }

		/*
		 * The 'rules' XML node is expected to contain at least one <screen>
		 * node. Subseqent <screen> nodes are ignored. The <screen> node may
		 * contain any number of <column> nodes. Each <column> node may contain
		 * any number of <row> nodes, which, in turn, can contain <column>
		 * nodes.
		 */
		void update_from_xml(Xml_node rules, Area screen_size)
		{
			_targets.for_each([&] (Registered<Target> &target) {
				destroy(_alloc, &target); });

			_rules.construct(_alloc, rules);

			/* targets are only visible on first screen */
			Target::Visible visible = Target::Visible::YES;

			rules.for_each_sub_node("screen", [&] (Xml_node const &screen) {

				Rect const avail(Point(0, 0), screen_size);

				if (screen.attribute_value("name", Target::Name()).valid())
					new (_alloc)
						Registered<Target>(_targets, screen, avail, visible);

				_process_rec(screen, avail, true, visible);

				visible = Target::Visible::NO;
			});
		}

		void gen_layout(Xml_generator &xml, Assign_list const &assignments) const
		{
			unsigned min_layer = 0;

			/* iterate over layers, starting at top-most layer (0) */
			for (;;) {

				unsigned const layer =
					_gen_top_most_layer(xml, min_layer, assignments);

				if (layer == MAX_LAYER)
					break;

				/* skip layer in next iteration */
				min_layer = layer + 1;
			}
		}

		/**
		 * Generate screen-layout definitions for the 'rules' report
		 *
		 * If a valid 'screen_name' is specified, move the referred screen in
		 * front of all others.
		 */
		void gen_screens(Xml_generator &xml, Target::Name const &screen_name) const
		{
			if (!_rules.constructed())
				return;

			xml.append("\n");
			_rules->xml().for_each_sub_node("screen", [&] (Xml_node screen) {
				if (screen_name.valid()) {
					Target::Name const name =
						screen.attribute_value("name", Target::Name());

					if (screen_name != name)
						return;
				}
				xml.append("\t");
				screen.with_raw_node([&] (char const *start, size_t length) {
					xml.append(start, length); });
				xml.append("\n");
			});

			if (!screen_name.valid())
				return;

			_rules->xml().for_each_sub_node("screen", [&] (Xml_node screen) {
				Target::Name const name = screen.attribute_value("name", Target::Name());
				if (screen_name == name)
					return;

				xml.append("\t");
				screen.with_raw_node([&] (char const *start, size_t length) {
					xml.append(start, length); });
				xml.append("\n");
			});
		}

		template <typename FN>
		void for_each(FN const &fn) const { _targets.for_each(fn); }
};

#endif /* _TARGET_LIST_H_ */
