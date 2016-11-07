/*
 * \brief  Menu view
 * \author Norman Feske
 * \date   2009-09-11
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _WIDGETS_H_
#define _WIDGETS_H_

/* Genode includes */
#include <util/xml_generator.h>
#include <timer_session/connection.h>

/* demo includes */
#include <scout_gfx/icon_painter.h>
#include <util/lazy_value.h>

/* gems includes */
#include <gems/animator.h>
#include <gems/dither_painter.h>

/* local includes */
#include "style_database.h"

namespace Menu_view {

	struct Margin;
	struct Widget;
	struct Root_widget;
	struct Frame_widget;
	struct Button_widget;
	struct Label_widget;
	struct Box_layout_widget;
	struct Widget_factory;
	struct Main;

	typedef Margin Padding;
}


class Menu_view::Widget_factory
{
	private:

		unsigned _unique_id_cnt = 0;

	public:

		Allocator      &alloc;
		Style_database &styles;
		Animator       &animator;

		Widget_factory(Allocator &alloc, Style_database &styles, Animator &animator)
		:
			alloc(alloc), styles(styles), animator(animator)
		{ }

		Widget *create(Xml_node node);

		void destroy(Widget *widget) { Genode::destroy(alloc, widget); }
};


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
			Widget *w = _children.first();

			unsigned const num_sub_nodes = node.num_sub_nodes();

			/* remove widget of vanished child */
			if (w && num_sub_nodes == 0)
				_remove_child(w);

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


struct Menu_view::Root_widget : Widget
{
	Root_widget(Widget_factory &factory, Xml_node node, Unique_id unique_id)
	:
		Widget(factory, node, unique_id)
	{ }

	void update(Xml_node node) override
	{
		char const *dialog_tag = "dialog";

		if (!node.has_type(dialog_tag)) {
			Genode::error("no valid <dialog> tag found");
			return;
		}

		if (!node.num_sub_nodes()) {
			Genode::warning("empty <dialog> node");
			return;
		}

		_update_child(node);
	}

	Area min_size() const override
	{
		if (Widget const * const child = _children.first())
			return child->min_size();

		return Area(1, 1);
	}

	void draw(Surface<Pixel_rgb888> &pixel_surface,
	          Surface<Pixel_alpha8> &alpha_surface,
	          Point at) const
	{
		_draw_children(pixel_surface, alpha_surface, at);
	}

	void _layout() override
	{
		if (Widget *child = _children.first())  {
			child->size(geometry.area());
			child->position(Point(0, 0));
		}
	}
};


struct Menu_view::Frame_widget : Widget
{
	Texture<Pixel_rgb888> const * texture = nullptr;

	Padding padding {  2,  2, 2, 2 };

	Area _space() const
	{
		return Area(margin.horizontal() + padding.horizontal(),
		            margin.vertical()   + padding.vertical());
	}

	Frame_widget(Widget_factory &factory, Xml_node node, Unique_id unique_id)
	:
		Widget(factory, node, unique_id)
	{
		margin = { 4, 4, 4, 4 };
	}

	void update(Xml_node node) override
	{
		texture = _factory.styles.texture(node, "background");

		_update_child(node);

		/*
		 * layout
		 */
		if (Widget *child = _children.first())
			child->geometry = Rect(Point(margin.left + padding.left,
			                             margin.top  + padding.top),
			                       child->min_size());
	}

	Area min_size() const override
	{
		/* determine minimum child size */
		Widget const * const child = _children.first();
		Area const child_min_size = child ? child->min_size() : Area(0, 0);

		/* don't get smaller than the background texture */
		Area const texture_size = texture ? texture->size() : Area(0, 0);

		return Area(max(_space().w() + child_min_size.w(), texture_size.w()),
		            max(_space().h() + child_min_size.h(), texture_size.h()));
	}

	void draw(Surface<Pixel_rgb888> &pixel_surface,
	          Surface<Pixel_alpha8> &alpha_surface,
	          Point at) const
	{
		Icon_painter::paint(pixel_surface, Rect(at, geometry.area()),
		                    *texture, 255);

		Icon_painter::paint(alpha_surface, Rect(at, geometry.area()),
		                    *texture, 255);

		_draw_children(pixel_surface, alpha_surface, at);
	}

	void _layout() override
	{
		if (Widget *child = _children.first())
			child->size(Area(geometry.w() - _space().w(),
			                 geometry.h() - _space().h()));
	}
};


struct Menu_view::Box_layout_widget : Widget
{
	Area _min_size; /* value cached from layout computation */

	enum Direction { VERTICAL, HORIZONTAL };

	Direction const _direction;

	Box_layout_widget(Widget_factory &factory, Xml_node node, Unique_id unique_id)
	:
		Widget(factory, node, unique_id),
		       _direction(node.has_type("vbox") ? VERTICAL : HORIZONTAL)
	{ }

	void update(Xml_node node) override
	{
		_update_children(node);

		/*
		 * Apply layout to the children
		 */

		/* determine largest size among our children */
		unsigned largest_size = 0;
		for (Widget *w = _children.first(); w; w = w->next())
			largest_size =
				max(largest_size, _direction == VERTICAL ? w->min_size().w()
				                                         : w->min_size().h());

		/* position children on one row/column */
		Point position(0, 0);
		for (Widget *w = _children.first(); w; w = w->next()) {

			Area const child_min_size = w->min_size();

			if (_direction == VERTICAL) {
				w->geometry = Rect(position, Area(largest_size, child_min_size.h()));
				unsigned const next_top_margin = w->next() ? w->next()->margin.top : 0;
				unsigned const dy = child_min_size.h() - min(w->margin.bottom, next_top_margin);
				position = position + Point(0, dy);
			} else {
				w->geometry = Rect(position, Area(child_min_size.w(), largest_size));
				unsigned const next_left_margin = w->next() ? w->next()->margin.left : 0;
				unsigned const dx = child_min_size.w() - min(w->margin.right, next_left_margin);
				position = position + Point(dx, 0);
			}

			_min_size = Area(w->geometry.x2() + 1, w->geometry.y2() + 1);
		}
	}

	Area min_size() const override
	{
		return _min_size;
	}

	void draw(Surface<Pixel_rgb888> &pixel_surface,
	          Surface<Pixel_alpha8> &alpha_surface,
	          Point at) const
	{
		_draw_children(pixel_surface, alpha_surface, at);
	}

	void _layout() override
	{
		for (Widget *w = _children.first(); w; w = w->next()) {
			if (_direction == VERTICAL)
				w->size(Area(geometry.w(), w->min_size().h()));
			else
				w->size(Area(w->min_size().w(), geometry.h()));
		}
	}
};


namespace Menu_view { template <typename PT> class Scratch_surface; }


template <typename PT>
class Menu_view::Scratch_surface
{
	private:

		Area           _size;
		Allocator     &_alloc;
		unsigned char *_base = nullptr;
		size_t         _num_bytes = 0;

		size_t _needed_bytes(Area size)
		{
			/* account for pixel buffer and alpha channel */
			return size.count()*sizeof(PT) + size.count();
		}

		void _release()
		{
			if (_base) {
				_alloc.free(_base, _num_bytes);
				_base = nullptr;
			}
		}

		unsigned char *_pixel_base() const { return _base; }

		unsigned char *_alpha_base() const
		{
			return _base + _size.count()*sizeof(PT);
		}

	public:

		Scratch_surface(Allocator &alloc) : _alloc(alloc) { }

		~Scratch_surface()
		{
			_release();
		}

		void reset(Area size)
		{
			if (_num_bytes < _needed_bytes(size)) {
				_release();

				_size      = size;
				_num_bytes = _needed_bytes(size);
				_base      = (unsigned char *)_alloc.alloc(_num_bytes);
			}

			Genode::memset(_base, 0, _num_bytes);
		}

		Surface<PT> pixel_surface() const
		{
			return Surface<PT>((PT *)_pixel_base(), _size);
		}

		Surface<Pixel_alpha8> alpha_surface() const
		{
			return Surface<Pixel_alpha8>((Pixel_alpha8 *)_alpha_base(), _size);
		}

		Texture<PT> texture() const
		{
			return Texture<PT>((PT *)_pixel_base(), _alpha_base(), _size);
		}
};


struct Menu_view::Button_widget : Widget, Animator::Item
{
	bool hovered  = false;
	bool selected = false;

	Texture<Pixel_rgb888> const * default_texture = nullptr;
	Texture<Pixel_rgb888> const * hovered_texture = nullptr;

	Lazy_value<int> blend;

	Padding padding { 9, 9, 2, 1 };

	Area _space() const
	{
		return Area(margin.horizontal() + padding.horizontal(),
		            margin.vertical()   + padding.vertical());
	}

	static bool _enabled(Xml_node node, char const *attr)
	{
		return node.attribute_value(attr, false);
	}

	Button_widget(Widget_factory &factory, Xml_node node, Unique_id unique_id)
	:
		Widget(factory, node, unique_id), Animator::Item(factory.animator)
	{
		margin = { 4, 4, 4, 4 };
	}

	void update(Xml_node node)
	{
		bool const new_hovered  = _enabled(node, "hovered");
		bool const new_selected = _enabled(node, "selected");

		if (new_selected) {
			default_texture = _factory.styles.texture(node, "selected");
			hovered_texture = _factory.styles.texture(node, "hselected");
		} else {
			default_texture = _factory.styles.texture(node, "default");
			hovered_texture = _factory.styles.texture(node, "hovered");
		}

		if (new_hovered != hovered) {

			if (new_hovered) {
				blend.dst(255 << 8, 3);
			} else {
				blend.dst(0, 20);
			}
			animated(blend != blend.dst());
		}

		hovered  = new_hovered;
		selected = new_selected;

		_update_child(node);

		bool const dy = selected ? 1 : 0;

		if (Widget *child = _children.first())
			child->geometry = Rect(Point(margin.left + padding.left,
			                             margin.top  + padding.top + dy),
			                       child->min_size());
	}

	Area min_size() const override
	{
		/* determine minimum child size */
		Widget const * const child = _children.first();
		Area const child_min_size = child ? child->min_size() : Area(300, 10);

		/* don't get smaller than the background texture */
		Area const texture_size = default_texture->size();

		return Area(max(_space().w() + child_min_size.w(), texture_size.w()),
		            max(_space().h() + child_min_size.h(), texture_size.h()));
	}

	void draw(Surface<Pixel_rgb888> &pixel_surface,
	          Surface<Pixel_alpha8> &alpha_surface,
	          Point at) const
	{
		static Scratch_surface<Pixel_rgb888> scratch(_factory.alloc);

		Area const texture_size = default_texture->size();
		Rect const texture_rect(Point(0, 0), texture_size);

		/*
		 * Mix from_texture and to_texture according to the blend value
		 */
		scratch.reset(texture_size);

		Surface<Pixel_rgb888> scratch_pixel_surface = scratch.pixel_surface();
		Surface<Pixel_alpha8> scratch_alpha_surface = scratch.alpha_surface();

		Icon_painter::paint(scratch_pixel_surface, texture_rect, *default_texture, 255);
		Icon_painter::paint(scratch_alpha_surface, texture_rect, *default_texture, 255);

		Icon_painter::paint(scratch_pixel_surface, texture_rect, *hovered_texture, blend >> 8);
		Icon_painter::paint(scratch_alpha_surface, texture_rect, *hovered_texture, blend >> 8);

		/*
		 * Apply blended texture to target surface
		 */
		Icon_painter::paint(pixel_surface, Rect(at, geometry.area()),
		                    scratch.texture(), 255);

		Icon_painter::paint(alpha_surface, Rect(at, geometry.area()),
		                    scratch.texture(), 255);

		_draw_children(pixel_surface, alpha_surface, at);
	}

	void _layout() override
	{
		for (Widget *w = _children.first(); w; w = w->next())
			w->size(Area(geometry.w() - _space().w(),
			             geometry.h() - _space().h()));
	}


	/******************************
	 ** Animator::Item interface **
	 ******************************/

	void animate() override
	{
		blend.animate();

		animated(blend != blend.dst());
	}
};


struct Menu_view::Label_widget : Widget
{
	Text_painter::Font const *font = nullptr;

	enum { LABEL_MAX_LEN = 256 };

	typedef String<200> Text;
	Text text;

	Label_widget(Widget_factory &factory, Xml_node node, Unique_id unique_id)
	:
		Widget(factory, node, unique_id)
	{ }

	void update(Xml_node node)
	{
		font = _factory.styles.font(node, "font");
		text = Decorator::string_attribute(node, "text", Text(""));
	}

	Area min_size() const override
	{
		if (!font)
			return Area(0, 0);

		return Area(font->str_w(text.string()),
		            font->str_h(text.string()));
	}

	void draw(Surface<Pixel_rgb888> &pixel_surface,
	          Surface<Pixel_alpha8> &alpha_surface,
	          Point at) const
	{
		if (!font) return;

		Area text_size = min_size();

		int const dx = (int)geometry.w() - text_size.w(),
		          dy = (int)geometry.h() - text_size.h();

		Point const centered = Point(dx/2, dy/2);

		Text_painter::paint(pixel_surface, at + centered, *font,
		                    Color(0, 0, 0), text.string());
	}
};


Menu_view::Widget *
Menu_view::Widget_factory::create(Xml_node node)
{
	Widget *w = nullptr;

	Widget::Unique_id const unique_id(++_unique_id_cnt);

	if (node.has_type("label"))  w = new (alloc) Label_widget      (*this, node, unique_id);
	if (node.has_type("button")) w = new (alloc) Button_widget     (*this, node, unique_id);
	if (node.has_type("vbox"))   w = new (alloc) Box_layout_widget (*this, node, unique_id);
	if (node.has_type("hbox"))   w = new (alloc) Box_layout_widget (*this, node, unique_id);
	if (node.has_type("frame"))  w = new (alloc) Frame_widget      (*this, node, unique_id);

	if (!w) {
		char type[64];
		type[0] = 0;
		node.type_name(type, sizeof(type));
		Genode::error("unknown widget type '", Cstring(type), "'");
		return 0;
	}

	return w;
}

#endif /* _WIDGETS_H_ */
