/*
 * \brief  Domain registry
 * \author Norman Feske
 * \date   2014-06-12
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DOMAIN_REGISTRY_
#define _DOMAIN_REGISTRY_

#include "types.h"

namespace Nitpicker { class Domain_registry; }


class Nitpicker::Domain_registry
{
	public:

		class Entry : public List<Entry>::Element
		{
			public:

				typedef String<64> Name;

				enum Label   { LABEL_NO, LABEL_YES };
				enum Content { CONTENT_CLIENT, CONTENT_TINTED };
				enum Hover   { HOVER_FOCUSED, HOVER_ALWAYS };
				enum Focus   { FOCUS_NONE, FOCUS_CLICK, FOCUS_TRANSIENT };

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

				Name      _name;
				Color     _color;
				Label     _label;
				Content   _content;
				Hover     _hover;
				Focus     _focus;
				Origin    _origin;
				unsigned  _layer;
				Point     _offset;
				Point     _area;

				friend class Domain_registry;

				Entry(Name const &name, Color color, Label label,
				      Content content, Hover hover, Focus focus,
				      Origin origin, unsigned layer, Point offset, Point area)
				:
					_name(name), _color(color), _label(label),
					_content(content), _hover(hover), _focus(focus),
					_origin(origin), _layer(layer), _offset(offset), _area(area)
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

				Name      name()      const { return _name;    }
				Color     color()     const { return _color;   }
				unsigned  layer()     const { return _layer;   }
				Content   content()   const { return _content; }
				Hover     hover()     const { return _hover;   }

				bool label_visible()   const { return _label == LABEL_YES; }
				bool content_client()  const { return _content == CONTENT_CLIENT; }
				bool hover_focused()   const { return _hover == HOVER_FOCUSED; }
				bool hover_always()    const { return _hover == HOVER_ALWAYS; }
				bool focus_click()     const { return _focus == FOCUS_CLICK; }
				bool focus_transient() const { return _focus == FOCUS_TRANSIENT; }
				bool origin_pointer()  const { return _origin == ORIGIN_POINTER; }

				Point phys_pos(Point pos, Area screen_area) const
				{
					return pos + _corner(screen_area) + _offset;
				}

				Area screen_area(Area phys_screen_area) const
				{
					int const w = _area.x() > 0
					            ? _area.x()
					            : max(0, (int)phys_screen_area.w() + _area.x());

					int const h = _area.y() > 0
					            ? _area.y()
					            : max(0, (int)phys_screen_area.h() + _area.y());

					return Area(w, h);
				}
		};

		static Entry::Label _label(Xml_node domain)
		{
			typedef String<32> Value;
			Value const value = domain.attribute_value("label", Value("yes"));

			if (value == "no")  return Entry::LABEL_NO;
			if (value == "yes") return Entry::LABEL_YES;

			warning("invalid value of label attribute in <domain>");
			return Entry::LABEL_YES;
		}

		static Entry::Content _content(Xml_node domain)
		{
			typedef String<32> Value;
			Value const value = domain.attribute_value("content", Value("tinted"));

			if (value == "client") return Entry::CONTENT_CLIENT;
			if (value == "tinted") return Entry::CONTENT_TINTED;

			return Entry::CONTENT_TINTED;
		}

		static Entry::Hover _hover(Xml_node domain)
		{
			typedef String<32> Value;
			Value const value = domain.attribute_value("hover", Value("focused"));

			if (value == "focused") return Entry::HOVER_FOCUSED;
			if (value == "always")  return Entry::HOVER_ALWAYS;

			warning("invalid value of hover attribute in <domain>");
			return Entry::HOVER_FOCUSED;
		}

		static Entry::Focus _focus(Xml_node domain)
		{
			typedef String<32> Value;
			Value const value = domain.attribute_value("focus", Value("none"));

			if (value == "none")      return Entry::FOCUS_NONE;
			if (value == "click")     return Entry::FOCUS_CLICK;
			if (value == "transient") return Entry::FOCUS_TRANSIENT;

			warning("invalid value of focus attribute in <domain>");
			return Entry::FOCUS_NONE;
		}

		static Entry::Origin _origin(Xml_node domain)
		{
			typedef String<32> Value;
			Value const value = domain.attribute_value("origin", Value("top_left"));

			if (value == "top_left")     return Entry::ORIGIN_TOP_LEFT;
			if (value == "top_right")    return Entry::ORIGIN_TOP_RIGHT;
			if (value == "bottom_left")  return Entry::ORIGIN_BOTTOM_LEFT;
			if (value == "bottom_right") return Entry::ORIGIN_BOTTOM_RIGHT;
			if (value == "pointer")      return Entry::ORIGIN_POINTER;

			warning("invalid value of origin attribute in <domain>");
			return Entry::ORIGIN_BOTTOM_LEFT;
		}

		void _insert(Xml_node domain)
		{
			char buf[sizeof(Entry::Name)];
			buf[0] = 0;
			bool name_defined = false;
			try {
				domain.attribute("name").value(buf, sizeof(buf));
				name_defined = true;
			} catch (...) { }

			if (!name_defined) {
				error("no valid domain name specified");
				return;
			}

			Entry::Name const name(buf);

			if (lookup(name)) {
				error("domain name \"", name, "\" is not unique");
				return;
			}

			if (!domain.has_attribute("layer")) {
				error("no layer specified for domain \"", name, "\"");
				return;
			}

			unsigned const layer = domain.attribute_value("layer", ~0UL);

			Point const offset(domain.attribute_value("xpos", 0L),
			                   domain.attribute_value("ypos", 0L));

			Point const area(domain.attribute_value("width",  0L),
			                 domain.attribute_value("height", 0L));

			Color const color = domain.attribute_value("color", white());

			_entries.insert(new (_alloc) Entry(name, color, _label(domain),
			                                   _content(domain), _hover(domain),
			                                   _focus(domain),
			                                   _origin(domain), layer, offset, area));
		}

	private:

		List<Entry> _entries { };
		Allocator  &_alloc;

	public:

		Domain_registry(Allocator &alloc, Xml_node config)
		:
			_alloc(alloc)
		{
			char const *type = "domain";

			if (!config.has_sub_node(type))
				return;

			Xml_node domain = config.sub_node(type);

			for (;; domain = domain.next(type)) {

				_insert(domain);

				if (domain.last(type))
					break;
			}
		}

		~Domain_registry()
		{
			while (Entry *e = _entries.first()) {
				_entries.remove(e);
				destroy(_alloc, e);
			}
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
