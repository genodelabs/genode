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

/* local includes */
#include <widget_factory.h>
#include <animated_geometry.h>

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


class Menu_view::Widget : public List_model<Widget>::Element
{
	public:

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

			bool operator != (Unique_id const &other) { return other.value != value; }

			bool valid() const { return value != 0; }
		};

		static Type_name node_type_name(Xml_node node)
		{
			char type[NAME_MAX_LEN];
			node.type_name(type, sizeof(type));

			return Type_name(Cstring(type));
		}

		static Name node_name(Xml_node node)
		{
			return Decorator::string_attribute(node, "name", node_type_name(node));
		}

	private:

		Type_name const _type_name;
		Name      const _name;
		Version   const _version;

		Unique_id const _unique_id;

	protected:

		Widget_factory &_factory;

		List_model<Widget> _children;

		struct Model_update_policy : List_model<Widget>::Update_policy
		{
			Widget_factory &_factory;

			Model_update_policy(Widget_factory &factory) : _factory(factory) { }

			void destroy_element(Widget &w) { _factory.destroy(&w); }

			Widget &create_element(Xml_node elem_node)
			{
				if (Widget *w = _factory.create(elem_node))
					return *w;

				throw Unknown_element_type();
			}

			void update_element(Widget &w, Xml_node node) { w.update(node); }

			static bool element_matches_xml_node(Widget const &w, Xml_node node)
			{
				return node.has_type(w._type_name.string())
				    && Widget::node_name(node) == w._name
				    && node.attribute_value("version", Version()) == w._version;
			}

		} _model_update_policy { _factory };

		inline void _update_children(Xml_node node)
		{
			_children.update_from_xml(_model_update_policy, node);
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
		Rect _geometry;

		Animated_rect _animated_geometry { _factory.animator };

		void _trigger_geometry_animation()
		{
			if (_animated_geometry.animated())
				return;

			if (_geometry.p1() != _animated_geometry.p1()
			 || _geometry.p2() != _animated_geometry.p2())
				_animated_geometry.move_to(_geometry, Animated_rect::Steps{60});
		}

	public:

		Margin margin { 0, 0, 0, 0 };

		void geometry(Rect geometry)
		{
			_geometry = geometry;
			_trigger_geometry_animation();
		}

		Rect geometry() const { return _geometry; }

		Rect animated_geometry() const { return _animated_geometry; }

		/*
		 * Return x/y positions of the edges of the widget with the margin
		 * applied
		 */
		Rect edges() const
		{
			Rect const r = _animated_geometry;
			return Rect(Point(r.x1() + margin.left,  r.y1() + margin.top),
		                Point(r.x2() - margin.right, r.y2() - margin.bottom));
		}

		Widget(Widget_factory &factory, Xml_node node, Unique_id unique_id)
		:
			_type_name(node_type_name(node)),
			_name(node_name(node)),
			_unique_id(unique_id),
			_factory(factory)
		{ }

		virtual ~Widget()
		{
			_children.destroy_all_elements(_model_update_policy);
		}

		bool has_name(Name const &name) const { return name == _name; }

		virtual void update(Xml_node node) = 0;

		virtual Area min_size() const = 0;

		virtual void draw(Surface<Pixel_rgb888> &pixel_surface,
		                  Surface<Pixel_alpha8> &alpha_surface,
		                  Point at) const = 0;

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

		/**
		 * Return unique ID of inner-most hovered widget
		 *
		 * This function is used to track changes of the hover model.
		 */
		virtual Unique_id hovered(Point at) const
		{
			if (!_inner_geometry().contains(at))
				return Unique_id();

			Unique_id result = _unique_id;
			_children.for_each([&] (Widget const &w) {
				Unique_id const id = w.hovered(at - w.geometry().p1());
				if (id.valid())
					result = id;
			});

			return result;
		}

		void print(Output &out) const
		{
			Genode::print(out, _name);
		}

		virtual void gen_hover_model(Xml_generator &xml, Point at) const
		{
			if (_inner_geometry().contains(at)) {

				xml.node(_type_name.string(), [&]() {

					xml.attribute("name",   _name.string());
					xml.attribute("xpos",   geometry().x1());
					xml.attribute("ypos",   geometry().y1());
					xml.attribute("width",  geometry().w());
					xml.attribute("height", geometry().h());

					_children.for_each([&] (Widget const &w) {
						w.gen_hover_model(xml, at - w.geometry().p1()); });
				});
			}
		}
};

#endif /* _WIDGET_H_ */
