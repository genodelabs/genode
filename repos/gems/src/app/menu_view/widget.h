/*
 * \brief  Common base class for all widgets
 * \author Norman Feske
 * \date   2009-09-11
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _WIDGET_H_
#define _WIDGET_H_

/* Genode includes */
#include <util/xml_generator.h>
#include <util/list_model.h>
#include <gems/animated_geometry.h>

/* local includes */
#include <widget_factory.h>

namespace Menu_view {

	struct Margin;
	struct Widget;

	typedef Margin Padding;
}


struct Menu_view::Margin
{
	unsigned left, right, top, bottom;

	Margin(unsigned left, unsigned right, unsigned top, unsigned bottom)
	:
		left(left), right(right), top(top), bottom(bottom)
	{ }

	unsigned horizontal() const { return left + right; }
	unsigned vertical()   const { return top + bottom; }
};


class Menu_view::Widget : List_model<Widget>::Element
{
	private:

		friend class List_model<Widget>;
		friend class List<Widget>;

	public:

		using List_model<Widget>::Element::next;

		enum { NAME_MAX_LEN = 32 };
		typedef String<NAME_MAX_LEN> Name;

		typedef Name       Type_name;
		typedef String<10> Version;

		struct Unique_id
		{
			unsigned value = 0;

			/**
			 * Constructor
			 *
			 * Only to be called by widget factory.
			 */
			Unique_id(unsigned value) : value(value) { }

			/**
			 * Default constructor creates invalid ID
			 */
			Unique_id() { }

			bool operator != (Unique_id const &other) const
			{
				return other.value != value;
			}

			bool valid() const { return value != 0; }
		};

		struct Hovered
		{
			/* widget */
			Unique_id unique_id;

			/* widget-local detail */
			using Detail = String<16>;

			Detail detail;

			bool operator != (Hovered const &other) const
			{
				return (unique_id != other.unique_id) || (detail != other.detail);
			}
		};

		static Name node_name(Xml_node const &node)
		{
			return node.attribute_value("name", Name(node.type()));
		}

		static Version node_version(Xml_node const &node)
		{
			return node.attribute_value("version", Version());
		}

		static Animated_rect::Steps motion_steps() { return { 60 }; };

	protected:

		Type_name const _type_name;
		Name      const _name;
		Version   const _version;

		Unique_id const _unique_id;

		Widget_factory &_factory;

		List_model<Widget> _children { };

		inline void _update_children(Xml_node node)
		{
			_children.update_from_xml(node,

				/* create */
				[&] (Xml_node const &node) -> Widget & {
					return _factory.create(node); },

				/* destroy */
				[&] (Widget &w) {
					_factory.destroy(&w); },

				/* update */
				[&] (Widget &w, Xml_node const &node) {
					w.update(node); }
			);
		}

		void _draw_children(Surface<Pixel_rgb888> &pixel_surface,
		                    Surface<Pixel_alpha8> &alpha_surface,
		                    Point at) const
		{
			_children.for_each([&] (Widget const &w) {
				w.draw(pixel_surface, alpha_surface, at + w._animated_geometry.p1()); });
		}

		virtual void _layout() { }

		Rect _inner_geometry() const
		{
			return Rect(Point(margin.left, margin.top),
			            Area(geometry().w() - margin.horizontal(),
			                 geometry().h() - margin.vertical()));
		}

		/*
		 * Position relative to the parent widget and actual size, defined by
		 * the parent
		 */
		Rect _geometry { Point(0, 0), Area(0, 0) };

		Animated_rect _animated_geometry { _factory.animator };

		void _trigger_geometry_animation()
		{
			bool const changed = (_geometry.p1() != _animated_geometry.p1()
			                   || _geometry.p2() != _animated_geometry.p2());
			if (changed)
				_animated_geometry.move_to(_geometry, motion_steps());
		}

		void _gen_common_hover_attr(Xml_generator &xml) const
		{
			xml.attribute("name",   _name.string());
			xml.attribute("xpos",   geometry().x1());
			xml.attribute("ypos",   geometry().y1());
			xml.attribute("width",  geometry().w());
			xml.attribute("height", geometry().h());
		}

	public:

		Margin margin { 0, 0, 0, 0 };

		Rect geometry() const { return _geometry; }

		Rect animated_geometry() const { return _animated_geometry.rect(); }

		/*
		 * Return x/y positions of the edges of the widget with the margin
		 * applied
		 */
		Rect edges() const
		{
			Rect const r = _animated_geometry.rect();
			return Rect(Point(r.x1() + margin.left,  r.y1() + margin.top),
		                Point(r.x2() - margin.right, r.y2() - margin.bottom));
		}

		Widget(Widget_factory &factory, Xml_node node, Unique_id unique_id)
		:
			_type_name(node.type()),
			_name(node_name(node)),
			_version(node_version(node)),
			_unique_id(unique_id),
			_factory(factory)
		{ }

		virtual ~Widget()
		{
			_update_children(Xml_node("<empty/>"));
		}

		bool has_name(Name const &name) const { return name == _name; }

		virtual void update(Xml_node node) = 0;

		virtual Area min_size() const = 0;

		virtual void draw(Surface<Pixel_rgb888> &pixel_surface,
		                  Surface<Pixel_alpha8> &alpha_surface,
		                  Point at) const = 0;

		/**
		 * Set widget size and update the widget tree's layout accordingly
		 */
		void size(Area size)
		{
			_geometry = Rect(_geometry.p1(), size);

			_layout();

			_trigger_geometry_animation();
		}

		void position(Point position)
		{
			_geometry = Rect(position, _geometry.area());
		}

		static Point _at_child(Point at, Widget const &w)
		{
			return at - w.geometry().p1();
		}

		static bool _child_hovered(Point at, Widget const &w)
		{
			return w.hovered(_at_child(at, w)).unique_id.valid();
		}

		bool _any_child_hovered(Point at) const
		{
			bool result = false;
			_children.for_each([&] (Widget const &w) {
				result = result | _child_hovered(at, w); });
			return result;
		}

		/**
		 * Return unique ID of inner-most hovered widget
		 *
		 * This function is used to track changes of the hover model.
		 */
		virtual Hovered hovered(Point at) const
		{
			if (!_inner_geometry().contains(at))
				return { };

			Hovered result { .unique_id = _unique_id, .detail = { } };

			if (!_any_child_hovered(at))
				return result;

			_children.for_each([&] (Widget const &w) {
				Hovered const hovered = w.hovered(_at_child(at, w));
				if (hovered.unique_id.valid())
					result = hovered; });

			return result;
		}

		virtual void gen_hover_model(Xml_generator &xml, Point at) const
		{
			if (!_inner_geometry().contains(at))
				return;

			xml.node(_type_name.string(), [&]() {

				_gen_common_hover_attr(xml);

				_children.for_each([&] (Widget const &w) {
					if (_child_hovered(at, w))
						w.gen_hover_model(xml, _at_child(at, w)); });
			});
		}

		void print(Output &out) const
		{
			Genode::print(out, _name);
		}

		/**
		 * List_model::Element
		 */
		bool matches(Xml_node const &node) const
		{
			return node.has_type(_type_name.string())
			    && Widget::node_name(node) == _name
			    && node_version(node) == _version;
		}

		/**
		 * List_model::Element
		 */
		static bool type_matches(Xml_node const &node)
		{
			return Widget_factory::node_type_known(node);
		}
};

#endif /* _WIDGET_H_ */
