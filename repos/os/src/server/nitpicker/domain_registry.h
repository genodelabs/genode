/*
 * \brief  Domain registry
 * \author Norman Feske
 * \date   2014-06-12
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _DOMAIN_REGISTRY_
#define _DOMAIN_REGISTRY_

#include <util/xml_node.h>
#include <util/color.h>

class Domain_registry
{
	public:

		class Entry : public Genode::List<Entry>::Element
		{
			public:

				typedef Genode::String<64> Name;
				typedef Genode::Color      Color;

				/**
				 * Behaviour of the views of the domain when X-ray is activated
				 */
				enum Xray {
					XRAY_NO,     /* views are not subjected to X-ray mode */
					XRAY_FRAME,  /* views are tinted and framed */
					XRAY_OPAQUE, /* views are replaced by opaque domain color */
				};

				/**
				 * Origin of the domain's coordiate system
				 */
				enum Origin {
					ORIGIN_POINTER,
					ORIGIN_TOP_LEFT,
					ORIGIN_TOP_RIGHT,
					ORIGIN_BOTTOM_LEFT,
					ORIGIN_BOTTOM_RIGHT };

			private:

				Name     _name;
				Color    _color;
				Xray     _xray;
				Origin   _origin;
				unsigned _layer;
				Point    _offset;
				Point    _area;

				friend class Domain_registry;

				Entry(Name const &name, Color color, Xray xray, Origin origin,
				      unsigned layer, Point offset, Point area)
				:
					_name(name), _color(color), _xray(xray), _origin(origin),
					_layer(layer), _offset(offset), _area(area)
				{ }

				Point _corner(Area screen_area) const
				{
					switch (_origin) {
					case ORIGIN_POINTER:      return Point(0, 0);
					case ORIGIN_TOP_LEFT:     return Point(0, 0);
					case ORIGIN_TOP_RIGHT:    return Point(screen_area.w(), 0);
					case ORIGIN_BOTTOM_LEFT:  return Point(0, screen_area.h());
					case ORIGIN_BOTTOM_RIGHT: return Point(screen_area.w(),
					                                       screen_area.h());
					}
					return Point(0, 0);
				}

			public:

				bool has_name(Name const &name) const { return name == _name; }

				Name name() const { return _name; }

				Color color() const { return _color; }

				unsigned layer() const { return _layer; }

				bool xray_opaque()    const { return _xray == XRAY_OPAQUE; }
				bool xray_no()        const { return _xray == XRAY_NO; }
				bool origin_pointer() const { return _origin == ORIGIN_POINTER; }

				Point phys_pos(Point pos, Area screen_area) const
				{
					return pos + _corner(screen_area) + _offset;
				}

				Area screen_area(Area phys_screen_area) const
				{
					int const w = _area.x() > 0
					            ? _area.x()
					            : Genode::max(0, (int)phys_screen_area.w() + _area.x());

					int const h = _area.y() > 0
					            ? _area.y()
					            : Genode::max(0, (int)phys_screen_area.h() + _area.y());

					return Area(w, h);
				}
		};

		static Entry::Xray _xray(Genode::Xml_node domain)
		{
			char const * const attr_name = "xray";

			Entry::Xray const default_xray = Entry::XRAY_FRAME;

			if (!domain.has_attribute(attr_name))
				return default_xray;

			Genode::Xml_node::Attribute const attr = domain.attribute(attr_name);

			if (attr.has_value("no"))     return Entry::XRAY_NO;
			if (attr.has_value("frame"))  return Entry::XRAY_FRAME;
			if (attr.has_value("opaque")) return Entry::XRAY_OPAQUE;

			PWRN("invalid value of xray attribute");
			return default_xray;
		}

		Entry::Origin _origin(Genode::Xml_node domain)
		{
			char const * const attr_name = "origin";

			Entry::Origin const default_origin = Entry::ORIGIN_TOP_LEFT;

			if (!domain.has_attribute(attr_name))
				return default_origin;

			Genode::Xml_node::Attribute const attr = domain.attribute(attr_name);

			if (attr.has_value("top_left"))     return Entry::ORIGIN_TOP_LEFT;
			if (attr.has_value("top_right"))    return Entry::ORIGIN_TOP_RIGHT;
			if (attr.has_value("bottom_left"))  return Entry::ORIGIN_BOTTOM_LEFT;
			if (attr.has_value("bottom_right")) return Entry::ORIGIN_BOTTOM_RIGHT;
			if (attr.has_value("pointer"))      return Entry::ORIGIN_POINTER;

			PWRN("invalid value of origin attribute");
			return default_origin;
		}

		void _insert(Genode::Xml_node domain)
		{
			char buf[sizeof(Entry::Name)];
			buf[0] = 0;
			bool name_defined = false;
			try {
				domain.attribute("name").value(buf, sizeof(buf));
				name_defined = true;
			} catch (...) { }

			if (!name_defined) {
				PERR("no valid domain name specified");
				return;
			}

			Entry::Name const name(buf);

			if (lookup(name)) {
				PERR("domain name \"%s\" is not unique", name.string());
				return;
			}

			if (!domain.has_attribute("layer")) {
				PERR("no layer specified for domain \"%s\"", name.string());
				return;
			}

			unsigned const layer = domain.attribute_value("layer", ~0UL);

			Point const offset(domain.attribute_value("xpos", 0L),
			                   domain.attribute_value("ypos", 0L));

			Point const area(domain.attribute_value("width",  0L),
			                 domain.attribute_value("height", 0L));

			Entry::Color const color = domain.attribute_value("color", WHITE);

			_entries.insert(new (_alloc) Entry(name, color, _xray(domain),
			                                   _origin(domain), layer, offset, area));
		}

	private:

		Genode::List<Entry> _entries;
		Genode::Allocator  &_alloc;

	public:

		Domain_registry(Genode::Allocator &alloc, Genode::Xml_node config)
		:
			_alloc(alloc)
		{
			char const *type = "domain";

			if (!config.has_sub_node(type))
				return;

			Genode::Xml_node domain = config.sub_node(type);

			for (;; domain = domain.next(type)) {

				_insert(domain);

				if (domain.is_last(type))
					break;
			}
		}

		~Domain_registry()
		{
			while (Entry *e = _entries.first())
				Genode::destroy(_alloc, e);
		}

		Entry const *lookup(Entry::Name const &name) const
		{
			for (Entry const *e = _entries.first(); e; e = e->next())
				if (e->has_name(name))
					return e;
			return 0;
		}
};

#endif /* _DOMAIN_REGISTRY_ */
