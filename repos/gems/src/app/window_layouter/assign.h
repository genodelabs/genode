/*
 * \brief  Assignment of window-manager clients to target area
 * \author Norman Feske
 * \date   2018-09-27
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _ASSIGN_H_
#define _ASSIGN_H_

/* Genode includes */
#include <base/registry.h>

/* local includes */
#include <types.h>
#include <target.h>
#include <decorator_margins.h>

namespace Window_layouter { class Assign; }


class Window_layouter::Assign : public List_model<Assign>::Element
{
	public:

		/*
		 * Used for associating windows with assignments. Hosted in 'Window'
		 * objects.
		 */
		struct Member : Registry<Member>::Element
		{
			Window &window;

			Member(Registry<Member> &registry, Window &window)
			: Registry<Member>::Element(registry, *this), window(window) { }
		};

		Target::Name target_name { };

	private:

		Registry<Member> _members { };

		struct Label
		{
			using Value = String<80>;

			Value exact, prefix, suffix;

			static Label from_node(Node const &node)
			{
				return { .exact  = node.attribute_value("label",        Value()),
				         .prefix = node.attribute_value("label_prefix", Value()),
				         .suffix = node.attribute_value("label_suffix", Value()) };
			}

			bool operator == (Label const &other) const
			{
				return exact  == other.exact
				    && prefix == other.prefix
				    && suffix == other.suffix;
			}

			bool matches(Value const &label) const
			{
				if (exact.valid() && label == exact)
					return true;

				if (exact.valid())
					return false;

				bool const prefix_matches =
					prefix.valid() &&
					!strcmp(label.string(), prefix.string(), prefix.length() - 1);

				bool suffix_matches = false;
				if (label.length() >= suffix.length()) {
					size_t const offset = label.length() - suffix.length();
					suffix_matches = !strcmp(label.string() + offset, suffix.string());
				}

				return (!prefix.valid() || prefix_matches)
				    && (!suffix.valid() || suffix_matches);
			}

			void gen_attr(Xml_generator &xml) const
			{
				if (exact .valid()) xml.attribute("label",        exact);
				if (prefix.valid()) xml.attribute("label_prefix", prefix);
				if (suffix.valid()) xml.attribute("label_suffix", suffix);
			}
		};

		Label const _label;

		bool _pos_defined  = false;
		bool _xpos_any     = false;
		bool _ypos_any     = false;
		bool _size_defined = false;
		bool _maximized    = false;
		bool _visible      = false;

		Point _pos  { };
		Area  _size { };

	public:

		Assign(Node const &assign) : _label(Label::from_node(assign)) { }

		void update(Node const &assign)
		{
			target_name   = assign.attribute_value("target", Target::Name());
			_pos_defined  = assign.has_attribute("xpos")  && assign.has_attribute("ypos");
			_size_defined = assign.has_attribute("width") && assign.has_attribute("height");
			_maximized    = assign.attribute_value("maximized", false);
			_visible      = assign.attribute_value("visible", true);
			_xpos_any     = assign.attribute_value("xpos", String<20>()) == "any";
			_ypos_any     = assign.attribute_value("ypos", String<20>()) == "any";
			_pos          = Point::from_node(assign);
			_size         = Area::from_node(assign);
		}

		/**
		 * List_model::Element
		 */
		bool matches(Node const &node) const { return Label::from_node(node) == _label; }

		/**
		 * List_model::Element
		 */
		static bool type_matches(Node const &node) { return node.has_type("assign");
		}

		/**
		 * Calculate window geometry
		 */
		Rect window_geometry(unsigned win_id, Area client_size, Area target_size,
		                     Decorator_margins const &decorator_margins) const
		{
			if (!_pos_defined)
				return { .at = { }, .area = target_size };

			/* try to place new window such that it fits the target area */
			unsigned const max_x = max(1, int(target_size.w) - int(client_size.w)),
			               max_y = max(1, int(target_size.h) - int(client_size.h));

			Point const any_pos(150*win_id % max_x, 30 + (100*win_id % max_y));

			Point const pos(_xpos_any ? any_pos.x : _pos.x,
			                _ypos_any ? any_pos.y : _pos.y);

			Rect const inner(pos, _size_defined ? _size : client_size);
			Rect const outer = decorator_margins.outer_geometry(inner);

			return Rect(outer.p1(), outer.area);
		}

		bool maximized() const { return _maximized; }
		bool visible()   const { return _visible; }

		/**
		 * Call 'fn' with 'Registry<Member>' if label matches assignment
		 *
		 * This method is used for associating assignments to windows.
		 */
		void with_matching_members_registry(Label::Value const &label, auto const &fn)
		{
			if (_label.matches(label))
				fn(_members);
		}

		/**
		 * Used to generate <assign> nodes of windows captured via wildcard
		 */
		void for_each_wildcard_member(auto const &fn) const
		{
			/* skip non-wildcards */
			if (_label.exact.valid())
				return;

			_members.for_each([&] (Assign::Member const &member) { fn(member); });
		}

		/**
		 * Used to bring wildcard-matching windows to front
		 */
		void for_each_wildcard_member(auto const &fn)
		{
			if (_label.exact.valid())
				return;

			_members.for_each([&] (Assign::Member &member) { fn(member); });
		}

		bool floating() const { return _pos_defined; }

		bool wildcard() const { return !_label.exact.valid(); }

		/**
		 * Generate <assign> node
		 */
		void gen_assign_attr(Xml_generator &xml) const
		{
			_label.gen_attr(xml);
			xml.attribute("target", target_name);
		}

		void gen_geometry_attr(Xml_generator &xml) const
		{
			if (_pos_defined) {
				if (_xpos_any) xml.attribute("xpos", "any");
				else           xml.attribute("xpos", _pos.x);

				if (_ypos_any) xml.attribute("ypos", "any");
				else           xml.attribute("ypos", _pos.y);
			}

			if (_size_defined) {
				xml.attribute("width",  _size.w);
				xml.attribute("height", _size.h);
			}

			if (_maximized)
				xml.attribute("maximized", "yes");

			if (!_visible)
				xml.attribute("visible", "no");
		}

		struct Window_state
		{
			Rect geometry;
			bool maximized;
		};

		void gen_geometry_attr(Xml_generator &xml, Window_state const &window) const
		{
			Rect const rect = window.maximized ? Rect(_pos, _size) : window.geometry;

			if (_pos_defined) {
				xml.attribute("xpos",   rect.x1());
				xml.attribute("ypos",   rect.y1());
				xml.attribute("width",  rect.w());
				xml.attribute("height", rect.h());
			}

			if (window.maximized)
				xml.attribute("maximized", "yes");

			if (!_visible)
				xml.attribute("visible", "no");
		}

		void for_each_member(auto const &fn)       { _members.for_each(fn); }
		void for_each_member(auto const &fn) const { _members.for_each(fn); }
};

#endif /* _ASSIGN_H_ */
