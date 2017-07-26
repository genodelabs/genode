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


class Menu_view::Widget : public List<Widget>::Element
{
	public:

		enum { NAME_MAX_LEN = 32 };
		typedef String<NAME_MAX_LEN> Name;

		typedef Name Type_name;

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

	private:

		Type_name const _type_name;
		Name      const _name;

		Unique_id const _unique_id;

	protected:

		Widget_factory &_factory;

		List<Widget> _children;

		Widget *_lookup_child(Name const &name)
		{
			for (Widget *w = _children.first(); w; w = w->next())
				if (w->_name == name)
					return w;

			return nullptr;
		}

		static Type_name _node_type_name(Xml_node node)
		{
			char type[NAME_MAX_LEN];
			node.type_name(type, sizeof(type));

			return Type_name(Cstring(type));
		}

		static bool _named_sub_node_exists(Xml_node node, Name const &name)
		{
			bool result = false;

			node.for_each_sub_node([&] (Xml_node sub_node) {
				if (sub_node.attribute_value("name", Name()) == name)
					result = true; });

			return result;
		}

		static Name _node_name(Xml_node node)
		{
			return Decorator::string_attribute(node, "name", _node_type_name(node));
		}

		void _remove_child(Widget *w)
		{
			_children.remove(w);
			_factory.destroy(w);
		}

		void _update_child(Xml_node node)
		{
			unsigned const num_sub_nodes = node.num_sub_nodes();

			if (Widget *w = _children.first()) {

				/* remove widget of vanished child */
				if (num_sub_nodes == 0)
					_remove_child(w);

				/* remove child widget if type or name changed */
				if (num_sub_nodes > 0) {
					Xml_node const child_node = node.sub_node();
					if (child_node.type() != w->_type_name
					 || child_node.attribute_value("name", Name()) != w->_name)
						_remove_child(w);
				}
			}

			if (num_sub_nodes == 0)
				return;

			/* update exiting widgets and create new ones */
			{
				Xml_node const child_node = node.sub_node();
				Name const name = _node_name(child_node);
				Widget *w = _lookup_child(name);
				if (!w) {
					w = _factory.create(child_node);

					/* append after previously inserted widget */
					if (w)
						_children.insert(w);
				}

				if (w)
					w->update(child_node);
			}
		}

		void _update_children(Xml_node node)
		{
			/*
			 * Remove no-longer present widgets
			 */
			Widget *next = nullptr;
			for (Widget *w = _children.first(); w; w = next) {
				next = w->next();

				if (!_named_sub_node_exists(node, w->_name))
					_remove_child(w);
			}

			/*
			 * Create and update widgets
			 */
			for (unsigned i = 0; i < node.num_sub_nodes(); i++) {

				Xml_node const child_node = node.sub_node(i);

				Name const name = _node_name(child_node);

				Widget *w = _lookup_child(name);

				if (!w) {
					w = _factory.create(child_node);

					/* ignore unknown widget types */
					if (!w) continue;

					/* append after previously inserted widget */
					_children.insert(w);
				}

				if (w)
					w->update(child_node);
			}

			/*
			 * Sort widgets according to the order of sub nodes
			 */
			Widget *previous = 0;
			Widget *w        = _children.first();

			node.for_each_sub_node([&] (Xml_node node) {

				if (!w) {
				Genode::error("unexpected end of widget list during re-ordering");
					return;
				}

				Name const name = node.attribute_value("name", Name());

				if (w->_name != name) {
					w = _lookup_child(name);
					if (!w) {
					Genode::error("widget lookup unexpectedly failed during re-ordering");
						return;
					}

					_children.remove(w);
					_children.insert(w, previous);
				}

				previous = w;
				w        = w->next();
			});
		}

		void _draw_children(Surface<Pixel_rgb888> &pixel_surface,
		                    Surface<Pixel_alpha8> &alpha_surface,
		                    Point at) const
		{
			for (Widget const *w = _children.first(); w; w = w->next())
				w->draw(pixel_surface, alpha_surface, at + w->geometry.p1());
		}

		virtual void _layout() { }

		Rect _inner_geometry() const
		{
			return Rect(Point(margin.left, margin.top),
			            Area(geometry.w() - margin.horizontal(),
			                 geometry.h() - margin.vertical()));
		}


	public:

		Margin margin { 0, 0, 0, 0 };

		/*
		 * Position relative to the parent widget and actual size, defined by
		 * the parent
		 */
		Rect geometry;

		Widget(Widget_factory &factory, Xml_node node, Unique_id unique_id)
		:
			_type_name(_node_type_name(node)),
			_name(_node_name(node)),
			_unique_id(unique_id),
			_factory(factory)
		{ }

		virtual ~Widget()
		{
			while (Widget *w = _children.first()) {
				_children.remove(w);
				_factory.destroy(w);
			}
		}

		virtual void update(Xml_node node) = 0;

		virtual Area min_size() const = 0;

		virtual void draw(Surface<Pixel_rgb888> &pixel_surface,
		                  Surface<Pixel_alpha8> &alpha_surface,
		                  Point at) const = 0;

		void size(Area size)
		{
			geometry = Rect(geometry.p1(), size);

			_layout();
		}

		void position(Point position)
		{
			geometry = Rect(position, geometry.area());
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

			for (Widget const *w = _children.first(); w; w = w->next()) {
				Unique_id res = w->hovered(at - w->geometry.p1());
				if (res.valid())
					return res;
			}

			return _unique_id;
		}

		virtual void gen_hover_model(Xml_generator &xml, Point at) const
		{
			if (_inner_geometry().contains(at)) {

				xml.node(_type_name.string(), [&]() {

					xml.attribute("name",   _name.string());
					xml.attribute("xpos",   geometry.x1());
					xml.attribute("ypos",   geometry.y1());
					xml.attribute("width",  geometry.w());
					xml.attribute("height", geometry.h());

					for (Widget const *w = _children.first(); w; w = w->next()) {
						w->gen_hover_model(xml, at - w->geometry.p1());
					}
				});
			}
		}
};

#endif /* _WIDGET_H_ */
