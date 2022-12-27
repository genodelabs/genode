/*
 * \brief  Representation of CPU affinities
 * \author Norman Feske
 * \date   2013-08-07
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__AFFINITY_H_
#define _INCLUDE__BASE__AFFINITY_H_

#include <util/xml_node.h>

namespace Genode { class Affinity; }


/**
 * Affinity to CPU nodes
 *
 * The entity of CPU nodes is expected to form a grid where the Euclidean
 * distance between nodes roughly correlate to the locality of their
 * respective resources. Closely interacting processes are supposed to
 * perform best when using nodes close to each other. To allow a relatively
 * simple specification of such constraints, the affinity of a subsystem
 * (e.g., a process) to CPU nodes is expressed as a rectangle within the
 * grid of available CPU nodes. The dimensions of the grid are represented
 * by 'Affinity::Space'. The rectangle within the grid is represented by
 * 'Affinity::Location'.
 */
class Genode::Affinity
{
	public:

		class Location;

		/**
		 * Bounds of the affinity name space
		 *
		 * An 'Affinity::Space' defines the bounds of a Cartesian
		 * coordinate space that expresses the entity of available CPU
		 * nodes. The dimension values do not necessarily correspond to
		 * physical CPU numbers. They solely represent the range the
		 * 'Affinity::Location' is relative to.
		 */
		class Space
		{
			private:

				unsigned _width, _height;

			public:

				Space() : _width(0), _height(0) { }

				/**
				 * Construct a two-dimensional affinity space
				 */
				Space(unsigned width, unsigned height)
				: _width(width), _height(height) { }

				/**
				 * Constuct one-dimensional affinity space
				 */
				Space(unsigned size) : _width(size), _height(1) { }

				unsigned width()  const { return _width; }
				unsigned height() const { return _height; }
				unsigned total()  const { return _width*_height; }

				Space multiply(Space const &other) const
				{
					return Space(_width*other.width(), _height*other.height());
				}

				/**
				 * Return the location of the Nth CPU within the affinity
				 * space
				 *
				 * This method returns a valid location even if the index
				 * is larger than the number of CPUs in the space. In this
				 * case, the x and y coordinates are wrapped by the bounds
				 * of the space.
				 */
				inline Location location_of_index(int index) const;

				static Space from_xml(Xml_node const &node)
				{
					return Affinity::Space(node.attribute_value("width",  0U),
					                       node.attribute_value("height", 0U));
				}
		};


		/**
		 * Location within 'Space'
		 */
		class Location
		{
			private:

				int      _xpos  = 0,  _ypos  = 0;
				unsigned _width = 0, _height = 0;

			public:

				/**
				 * Default constructor creates invalid location
				 */
				Location() { }

				/**
				 * Constructor to express the affinity to a single CPU
				 */
				Location(int xpos, int ypos)
				: _xpos(xpos), _ypos(ypos), _width(1), _height(1) { }

				/**
				 * Constructor to express the affinity to a set of CPUs
				 */
				Location(int xpos, int ypos, unsigned width, unsigned height)
				: _xpos(xpos), _ypos(ypos), _width(width), _height(height) { }

				int      xpos()   const { return _xpos; }
				int      ypos()   const { return _ypos; }
				unsigned width()  const { return _width; }
				unsigned height() const { return _height; }

				Location multiply_position(Space const &space) const
				{
					return Location(_xpos*space.width(), _ypos*space.height(),
					                _width, _height);
				}

				Location transpose(int dx, int dy) const
				{
					return Location(_xpos + dx, _ypos + dy, _width, _height);
				}

				/**
				 * Return true if the location resides within 'space'
				 */
				bool within(Space const &space) const
				{
					int const x1 = _xpos,  x2 = _xpos + _width  - 1,
					          y1 = _ypos,  y2 = _ypos + _height - 1;

					return x1 >= 0 && x1 <= x2 && (unsigned)x2 < space.width()
					    && y1 >= 0 && y1 <= y2 && (unsigned)y2 < space.height();
				}

				static Location from_xml(Space const &space, Xml_node const &node)
				{
					/* if no position value is specified, select the whole row/column */
					unsigned const
						default_width  = node.has_attribute("xpos") ? 1 : space.width(),
						default_height = node.has_attribute("ypos") ? 1 : space.height();

					return Location(node.attribute_value("xpos",   0U),
					                node.attribute_value("ypos",   0U),
					                node.attribute_value("width",  default_width),
					                node.attribute_value("height", default_height));
				}

		};

	private:

		Space    _space    { };
		Location _location { };

	public:

		Affinity(Space const &space, Location const &location)
		: _space(space), _location(location) { }

		Affinity() { }

		Space    space()    const { return _space; }
		Location location() const { return _location; }
		bool     valid()    const { return _location.within(_space); }

		static Affinity from_xml(Xml_node const &node)
		{
			Affinity::Space    space    { };
			Affinity::Location location { };

			node.with_optional_sub_node("affinity", [&] (Xml_node const &node) {
				node.with_optional_sub_node("space", [&] (Xml_node const &node) {
					space = Space::from_xml(node); });
				node.with_optional_sub_node("location", [&] (Xml_node const &node) {
					location = Location::from_xml(space, node); });
			});

			return Affinity(space, location);
		}

		static Affinity unrestricted()
		{
			return Affinity(Space(1, 1), Location(0, 0, 1, 1));
		}

		/**
		 * Return location scaled to specified affinity space
		 */
		Location scale_to(Space const &space) const
		{
			if (_space.total() == 0)
				return Location();

			/*
			 * Calculate coordinates of rectangle corners
			 *
			 * P1 is the upper left corner, inside the rectangle.
			 * P2 is the lower right corner, outside the rectangle.
			 */
			int const x1 = _location.xpos(),
			          y1 = _location.ypos(),
			          x2 = _location.width()  + x1,
			          y2 = _location.height() + y1;

			/* scale corner positions */
			int const scaled_x1 = (x1*space.width())  / _space.width(),
			          scaled_y1 = (y1*space.height()) / _space.height(),
			          scaled_x2 = (x2*space.width())  / _space.width(),
			          scaled_y2 = (y2*space.height()) / _space.height();

			/* make sure to not scale the location size to zero */
			return Location(scaled_x1, scaled_y1,
			                max(scaled_x2 - scaled_x1, 1),
			                max(scaled_y2 - scaled_y1, 1));
		}
};


Genode::Affinity::Location Genode::Affinity::Space::location_of_index(int index) const
{
	return Location(index % _width, (index / _width) % _height, 1, 1);
}

#endif /* _INCLUDE__BASE__AFFINITY_H_ */
