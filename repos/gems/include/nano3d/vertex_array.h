/*
 * \brief  Vertex array
 * \author Norman Feske
 * \date   2010-09-27
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__NANO3D__VERTEX_ARRAY_H_
#define _INCLUDE__NANO3D__VERTEX_ARRAY_H_

#include <nano3d/sincos_frac16.h>

namespace Nano3d {

	template <typename> class Vec2;
	template <typename> class Vec3;

	typedef Vec3<int> Vertex;

	template <unsigned> class Vertex_array;
}


template <typename T>
class Nano3d::Vec2
{
	private:

		T _x, _y;

	public:

		Vec2() : _x(0), _y(0) { }

		Vec2(T x, T y) : _x(x), _y(y) { }

		T &x() { return _x; }
		T &y() { return _y; }

		T x() const { return _x; }
		T y() const { return _y; }

		void rotate(int sina, int cosa)
		{
			int x = _x*cosa - _y*sina;
			int y = _x*sina + _y*cosa;

			_x = x >> 16;
			_y = y >> 16;
		}
};


template <typename T>
class Nano3d::Vec3
{
	private:

		T _x, _y, _z;

	public:

		Vec3() : _x(0), _y(0), _z(0) { }

		Vec3(T x, T y, T z) : _x(x), _y(y), _z(z) { }

		T &x() { return _x; }
		T &y() { return _y; }
		T &z() { return _z; }

		T x() const { return _x; }
		T y() const { return _y; }
		T z() const { return _z; }
};


template <unsigned MAX_VERTICES>
class Nano3d::Vertex_array
{
	protected:

		Vertex _buf[MAX_VERTICES];

		void _rotate(int & (Vec3<int>::*x)(), int & (Vec3<int>::*y)(), int angle)
		{
			int sina = sin_frac16(angle);
			int cosa = cos_frac16(angle);

			Vertex *vertex = &_buf[0];

			for (unsigned i = 0; i < MAX_VERTICES; i++, vertex++) {

				Vec2<int> p((vertex->*x)(), (vertex->*y)());

				p.rotate(sina, cosa);

				(vertex->*x)() = p.x();
				(vertex->*y)() = p.y();
			}
		}

	public:

		/**
		 * Constructor
		 */
		Vertex_array() { }

		Vertex &operator [] (unsigned index) { return _buf[index]; }

		/**
		 * Rotate vertices around x, y, and z axis
		 */
		void rotate_x(int angle) { _rotate(&Vertex::y, &Vertex::z, angle); }
		void rotate_y(int angle) { _rotate(&Vertex::x, &Vertex::z, angle); }
		void rotate_z(int angle) { _rotate(&Vertex::x, &Vertex::y, angle); }

		/**
		 * Apply central projection to vertices
		 *
		 * FIXME: Add proper documentation of the parameters.
		 *
		 * \param z_shift   Recommended value is 1600
		 * \param distance  Recommended value is screen height
		 */
		void project(int z_shift, int distance)
		{
			Vertex *vertex = &_buf[0];
			for (unsigned i = 0; i < MAX_VERTICES; i++, vertex++) {

				int z = (vertex->z() >> 5) + z_shift - 1;

				/* avoid division by zero */
				if (z == 0) z += 1;

				vertex->x() = ((vertex->x() >> 5) * distance) / z;
				vertex->y() = ((vertex->y() >> 5) * distance) / z;
			}
		}

		/**
		 * Translate vertices
		 */
		void translate(int dx, int dy, int dz)
		{
			Vertex *vertex = &_buf[0];
			for (unsigned i = 0; i < MAX_VERTICES; i++, vertex++) {
				vertex->x() += dx;
				vertex->y() += dy;
				vertex->z() += dz;
			}
		}
};

#endif /* _INCLUDE__NANO3D__VERTEX_ARRAY_H_ */
